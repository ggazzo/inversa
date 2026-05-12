#pragma once

#include <Arduino.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/constants.h"
#include "../core/ThermalCalc.h"
#include "../models/MachineState.h"
#include "RTCPlugin.h"

// ─── Scheduler Plugin ───────────────────────────────────────
// "Be ready at HH:MM" feature.
// Calculates when to start heating based on:
// - Target time (HH:MM)
// - Target temperature
// - Current temperature
// - Water volume (affects heating time)
// - Heater power (configurable)
//
// Formula: time_to_heat = (volume * delta_temp * 4186) / (power * efficiency)
// Where: 4186 J/(kg*K) = specific heat of water
//        efficiency = ~0.85 for typical immersion heater

class SchedulerPlugin : public Plugin {
public:
    SchedulerPlugin(MachineState& state, RTCPlugin& rtc) 
        : _state(state), _rtc(rtc) {}

    const char* getName() const override { return "Scheduler"; }

    bool setup() override {
        // Subscribe to temperature changes to check if target reached
        bus().subscribe(EventType::TemperatureRead, [this](const Event& e) {
            if (_state.schedulerActive && _heatingStarted) {
                checkTargetReached(e.floatValue);
            }
        });

        DEBUG_PRINTLN("[Scheduler] Plugin initialized");
        return true;
    }

    void loop() override {
        if (!_state.schedulerActive) {
            return;
        }

        uint32_t currentMillis = millis();
        
        // Check every second
        if (currentMillis - _lastCheck < 1000) {
            return;
        }
        _lastCheck = currentMillis;

        // If not heating yet, check if it's time to start
        if (!_heatingStarted) {
            uint32_t now = _rtc.now();
            if (now >= _state.schedulerStartTime) {
                startHeating();
            } else {
                // Update status with time remaining until start
                updateWaitingStatus(now);
            }
        }
    }

    // ── Public API ───────────────────────────────────────────

    // Schedule to be ready at HH:MM with target temperature
    // volume: water volume in liters
    // Returns false if RTC not available or invalid parameters
    bool schedule(uint8_t hour, uint8_t minute, float targetTemp, float volumeLiters) {
        if (!_rtc.isAvailable()) {
            DEBUG_PRINTLN("[Scheduler] Cannot schedule - RTC not available");
            return false;
        }

        if (targetTemp < 20.0f || targetTemp > 100.0f) {
            DEBUG_PRINTLN("[Scheduler] Invalid target temperature");
            return false;
        }

        if (volumeLiters < 1.0f || volumeLiters > 100.0f) {
            DEBUG_PRINTLN("[Scheduler] Invalid volume (1-100L)");
            return false;
        }

        // Store parameters
        _state.schedulerTargetHour = hour;
        _state.schedulerTargetMinute = minute;
        _state.schedulerTargetTemp = targetTemp;
        _state.schedulerVolume = volumeLiters;

        // Calculate when to start heating
        uint32_t heatingTime = calculateHeatingTime(targetTemp, volumeLiters);
        
        // Get target time as unix timestamp
        DateTime now = _rtc.getDateTime();
        DateTime target(now.year(), now.month(), now.day(), hour, minute, 0);
        uint32_t targetTs = target.unixtime();
        
        // If target time already passed today, schedule for tomorrow
        if (targetTs <= _rtc.now()) {
            targetTs += 24 * 3600;
        }

        // Calculate start time (target - heating time - margin)
        uint32_t marginSec = SCHEDULER_MARGIN_MINUTES * 60;
        if (heatingTime + marginSec >= targetTs - _rtc.now()) {
            DEBUG_PRINTLN("[Scheduler] Not enough time to reach target");
            _state.schedulerStatus = "Tempo insuficiente";
            return false;
        }

        _state.schedulerStartTime = targetTs - heatingTime - marginSec;
        _state.schedulerActive = true;
        _heatingStarted = false;

        // Update status
        uint32_t waitSec = _state.schedulerStartTime - _rtc.now();
        char buf[64];
        snprintf(buf, sizeof(buf), "Aguardando %lu min para iniciar", waitSec / 60);
        _state.schedulerStatus = buf;

        DEBUG_PRINTF("[Scheduler] Set for %02d:%02d, temp=%.1f, vol=%.1f L\n", 
                    hour, minute, targetTemp, volumeLiters);
        DEBUG_PRINTF("[Scheduler] Heating time: %lu sec, start in: %lu sec\n", 
                    heatingTime, waitSec);

        bus().publish(EventType::SchedulerSet);
        return true;
    }

    // Cancel scheduler
    void cancel() {
        if (!_state.schedulerActive) {
            return;
        }

        bool wasHeating = _heatingStarted;
        _state.schedulerActive = false;
        _heatingStarted = false;
        _state.schedulerStatus = "";

        // Turn off heater if we had started it AND no other controller owns
        // it. P17 — if a recipe was queued by the scheduler and is now
        // executing, don't reset its mode behind its back.
        if (wasHeating) {
            bus().publish(EventType::HeaterStateChanged, false);
            if (gState.mode == OperatingMode::Manual) {
                gState.mode = OperatingMode::Idle;
            }
        }

        DEBUG_PRINTLN("[Scheduler] Cancelled");
        bus().publish(EventType::SchedulerCancelled);
    }

    // Check if scheduler is active
    bool isActive() const { return _state.schedulerActive; }
    bool isHeating() const { return _heatingStarted; }

    // Get estimated time to target (seconds)
    uint32_t getTimeToTarget() const {
        if (!_state.schedulerActive) return 0;
        
        uint32_t now = _rtc.now();
        DateTime target(_rtc.getDateTime().year(), _rtc.getDateTime().month(), 
                       _rtc.getDateTime().day(), 
                       _state.schedulerTargetHour, _state.schedulerTargetMinute, 0);
        uint32_t targetTs = target.unixtime();
        if (targetTs <= now) targetTs += 24 * 3600;
        
        return targetTs > now ? targetTs - now : 0;
    }

private:
    // Scheduler configuration constants
    static constexpr float HEATER_EFFICIENCY = 0.90f;          // 90% efficiency
    static constexpr uint8_t SCHEDULER_MARGIN_MINUTES = 5;     // Extra margin

    MachineState& _state;
    RTCPlugin& _rtc;
    uint32_t _lastCheck = 0;
    bool _heatingStarted = false;

    // Calculate time needed to heat water (seconds)
    // Now uses ThermalCalc with heat loss compensation
    uint32_t calculateHeatingTime(float targetTemp, float volumeLiters) {
        // Get current temperature
        float currentTemp = _state.currentTemp;
        if (currentTemp < 5.0f) currentTemp = _state.ambientTemp;  // Use ambient if sensor error
        
        if (targetTemp <= currentTemp) return 0;  // Already at or above target

        // Use thermal parameters from state (can be configured)
        float power = _state.heaterPowerWatts;
        float ambient = _state.ambientTemp;
        float diameter = _state.vesselDiameter;
        float heatCoeff = _state.heatLossCoeff;

        // Calculate surface area from volume and diameter
        float surfaceArea = ThermalCalc::cylinderSurfaceArea(volumeLiters, diameter);

        // Calculate heating time with heat loss compensation
        float timeSec = ThermalCalc::calculateHeatingTimeWithLoss(
            volumeLiters, power, currentTemp, targetTemp,
            ambient, surfaceArea, HEATER_EFFICIENCY, heatCoeff
        );

        return (uint32_t)timeSec;
    }

    // Start the heating process
    void startHeating() {
        _heatingStarted = true;

        // Set target temperature and start heating
        gState.targetTemp = _state.schedulerTargetTemp;
        // P17 — preserve mode if a higher-level controller already owns the
        // heater (e.g., a recipe is queued). Only claim Manual when idle.
        if (gState.mode == OperatingMode::Idle) {
            gState.mode = OperatingMode::Manual;
        }

        bus().publish(EventType::SetpointChanged, _state.schedulerTargetTemp);
        bus().publish(EventType::HeaterStateChanged, true);
        bus().publish(EventType::SchedulerStarting);

        _state.schedulerStatus = "Aquecendo...";

        DEBUG_PRINTF("[Scheduler] Started heating to %.1f C\n", _state.schedulerTargetTemp);
    }

    // Check if target temperature has been reached
    void checkTargetReached(float currentTemp) {
        // Consider reached if within 0.5C of target
        if (currentTemp >= _state.schedulerTargetTemp - 0.5f) {
            _state.schedulerActive = false;
            _state.schedulerStatus = "Pronto!";
            
            DEBUG_PRINTF("[Scheduler] Target reached at %.1f C\n", currentTemp);
            bus().publish(EventType::SchedulerReady);
        }
    }

    // Update status while waiting to start
    void updateWaitingStatus(uint32_t now) {
        uint32_t waitSec = _state.schedulerStartTime - now;
        uint32_t waitMin = waitSec / 60;
        
        char buf[64];
        if (waitMin >= 60) {
            snprintf(buf, sizeof(buf), "Inicia em %lu h %lu min", waitMin / 60, waitMin % 60);
        } else {
            snprintf(buf, sizeof(buf), "Inicia em %lu min", waitMin);
        }
        _state.schedulerStatus = buf;
    }
};
