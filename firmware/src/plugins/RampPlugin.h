#pragma once

#include <Arduino.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../models/MachineState.h"

// ─── Ramp Mode Plugin ───────────────────────────────────────
// Provides gradual temperature ramping (°C/min)
// When active, the actual setpoint is adjusted progressively
// toward the target temperature at the configured rate.

class RampPlugin : public Plugin {
public:
    const char* getName() const override { return "Ramp"; }

    bool setup() override {
        // Listen for target temp changes to start ramp
        bus().subscribe(EventType::SetpointChanged, [this](const Event& e) {
            _finalTarget = e.floatValue;
            if (_enabled && _ratePerMin > 0) {
                startRamp();
            }
        });

        // Listen for ramp config changes
        bus().subscribe(EventType::RampConfigChanged, [this](const Event& e) {
            // e.floatValue is rate in °C/min, 0 = disabled
            _ratePerMin = e.floatValue;
            _enabled = (_ratePerMin > 0);
            
            if (!_enabled) {
                stopRamp();
            }
        });

        DEBUG_PRINTLN("[Ramp] Initialized");
        return true;
    }

    void loop() override {
        if (!_ramping) return;

        uint32_t now = millis();
        uint32_t elapsed = now - _lastUpdate;
        if (elapsed < 1000) return;  // Update every second

        _lastUpdate = now;

        // Calculate temperature change for this interval
        float elapsedMin = elapsed / 60000.0f;
        float deltaTemp = _ratePerMin * elapsedMin;

        if (_currentSetpoint < _finalTarget) {
            // Ramping up
            _currentSetpoint = min(_currentSetpoint + deltaTemp, _finalTarget);
        } else if (_currentSetpoint > _finalTarget) {
            // Ramping down
            _currentSetpoint = max(_currentSetpoint - deltaTemp, _finalTarget);
        }

        // Publish the ramped setpoint (PIDPlugin will use this)
        bus().publish(EventType::RampedSetpointChanged, _currentSetpoint);

        // Update state for telemetry
        gState.rampActive = true;
        gState.rampTarget = _finalTarget;
        gState.rampCurrent = _currentSetpoint;
        gState.rampRate = _ratePerMin;

        // Check if ramp is complete
        if (abs(_currentSetpoint - _finalTarget) < 0.01f) {
            stopRamp();
            bus().publish(EventType::RampCompleted, _finalTarget);
            DEBUG_PRINTF("[Ramp] Completed at %.1f°C\n", _finalTarget);
        }
    }

    void setRate(float ratePerMin) {
        _ratePerMin = ratePerMin;
        _enabled = (ratePerMin > 0);
        gState.rampRate = ratePerMin;
        
        if (!_enabled && _ramping) {
            stopRamp();
        }
    }

    void startRamp() {
        if (_ratePerMin <= 0) return;
        
        _currentSetpoint = gState.currentTemp;  // Start from current temp
        _ramping = true;
        _lastUpdate = millis();
        gState.rampActive = true;
        
        DEBUG_PRINTF("[Ramp] Starting: %.1f -> %.1f at %.2f°C/min\n", 
                      _currentSetpoint, _finalTarget, _ratePerMin);
    }

    void stopRamp() {
        if (_ramping) {
            bus().publish(EventType::RampedSetpointChanged, _finalTarget);
        }
        _ramping = false;
        gState.rampActive = false;
    }

    bool isRamping() const { return _ramping; }
    float getCurrentSetpoint() const { return _currentSetpoint; }
    float getFinalTarget() const { return _finalTarget; }
    float getRate() const { return _ratePerMin; }

private:
    bool _enabled = false;
    bool _ramping = false;
    float _ratePerMin = 0.0f;      // °C per minute
    float _currentSetpoint = 0.0f;  // Current ramped setpoint
    float _finalTarget = 0.0f;      // Final target temperature
    uint32_t _lastUpdate = 0;
};
