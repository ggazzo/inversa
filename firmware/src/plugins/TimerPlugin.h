#pragma once

#include <Arduino.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/constants.h"
#include "../models/MachineState.h"

// Forward declaration
class RTCPlugin;

// ─── Timer Mode ─────────────────────────────────────────────
enum class TimerMode : uint8_t {
    Relative,   // Countdown from duration (e.g., "10 minutes")
    Absolute    // Alarm at specific time (e.g., "08:00")
};

// ─── Timer Plugin ───────────────────────────────────────────
// Generic countdown timer for brewing operations.
// - Relative timers (duration-based countdown)
// - Absolute timers (alarm at HH:MM, requires RTC)
// - Start, stop, pause, resume functionality
// - Publishes tick events every second

class TimerPlugin : public Plugin {
public:
    TimerPlugin(MachineState& state, RTCPlugin* rtc = nullptr) 
        : _state(state), _rtc(rtc) {}

    const char* getName() const override { return "Timer"; }

    bool setup() override {
        DEBUG_PRINTLN("[Timer] Plugin initialized");
        return true;
    }

    void loop() override {
        if (!_state.timerActive || _state.timerPaused) {
            return;
        }

        uint32_t currentMillis = millis();

        // Tick every second
        if (currentMillis - _lastTick >= 1000) {
            _lastTick = currentMillis;

            // For absolute timers, recalculate remaining from RTC
            if (getMode() == TimerMode::Absolute) {
                updateAbsoluteRemaining();
            } else {
                // Relative timer - just decrement
                if (_state.timerRemaining > 0) {
                    _state.timerRemaining--;
                }
            }

            bus().publish(EventType::TimerTick, (int)_state.timerRemaining);

            if (_state.timerRemaining == 0) {
                complete();
            }
        }
    }

    // ── Public API ───────────────────────────────────────────

    // Start timer with duration in seconds (relative mode)
    void start(uint32_t durationSec) {
        if (durationSec == 0) {
            DEBUG_PRINTLN("[Timer] Cannot start timer with 0 duration");
            return;
        }

        _state.timerMode = (uint8_t)TimerMode::Relative;
        _state.timerActive = true;
        _state.timerPaused = false;
        _state.timerTotal = durationSec;
        _state.timerRemaining = durationSec;
        _lastTick = millis();

        DEBUG_PRINTF("[Timer] Started: %lu seconds\n", durationSec);
        bus().publish(EventType::TimerStarted, (int)durationSec);
    }

    // Start timer with duration in minutes
    void startMinutes(uint32_t durationMin) {
        start(durationMin * 60);
    }

    // Stop timer
    void stop() {
        if (!_state.timerActive) {
            return;
        }

        _state.timerActive = false;
        _state.timerPaused = false;
        _state.timerRemaining = 0;

        DEBUG_PRINTLN("[Timer] Stopped");
        bus().publish(EventType::TimerCancelled);
    }

    // Pause timer
    void pause() {
        if (!_state.timerActive || _state.timerPaused) {
            return;
        }

        _state.timerPaused = true;
        DEBUG_PRINTLN("[Timer] Paused");
        bus().publish(EventType::TimerPaused);
    }

    // Resume timer
    void resume() {
        if (!_state.timerActive || !_state.timerPaused) {
            return;
        }

        _state.timerPaused = false;
        _lastTick = millis();
        DEBUG_PRINTLN("[Timer] Resumed");
        bus().publish(EventType::TimerResumed);
    }

    // Add time to running timer
    void addTime(int32_t seconds) {
        if (!_state.timerActive) {
            return;
        }

        if (seconds < 0 && (uint32_t)(-seconds) > _state.timerRemaining) {
            _state.timerRemaining = 0;
        } else {
            _state.timerRemaining += seconds;
        }
        _state.timerTotal += seconds;

        DEBUG_PRINTF("[Timer] Time adjusted by %d seconds, remaining: %lu\n", 
                    seconds, _state.timerRemaining);
    }

    // Check if timer is active
    bool isActive() const { return _state.timerActive; }
    bool isPaused() const { return _state.timerPaused; }
    uint32_t getRemaining() const { return _state.timerRemaining; }
    uint32_t getTotal() const { return _state.timerTotal; }

    // Get remaining time formatted as MM:SS or HH:MM:SS
    String getRemainingFormatted() const {
        uint32_t secs = _state.timerRemaining;
        uint32_t hours = secs / 3600;
        uint32_t mins = (secs % 3600) / 60;
        uint32_t s = secs % 60;

        char buf[12];
        if (hours > 0) {
            snprintf(buf, sizeof(buf), "%lu:%02lu:%02lu", hours, mins, s);
        } else {
            snprintf(buf, sizeof(buf), "%02lu:%02lu", mins, s);
        }
        return String(buf);
    }

    // ── Absolute Timer API ────────────────────────────────────

    // Set RTC reference (for absolute timers)
    void setRTC(RTCPlugin* rtc) { _rtc = rtc; }

    // Start absolute timer (alarm at HH:MM)
    // Returns false if RTC not available
    bool startAt(uint8_t hour, uint8_t minute) {
        if (!_rtc) {
            DEBUG_PRINTLN("[Timer] Cannot start absolute timer - no RTC");
            return false;
        }

        _state.timerMode = (uint8_t)TimerMode::Absolute;
        _state.timerAlarmHour = hour;
        _state.timerAlarmMinute = minute;
        
        // Calculate initial remaining seconds
        if (!updateAbsoluteRemaining()) {
            DEBUG_PRINTLN("[Timer] Alarm time already passed");
            return false;
        }

        _state.timerActive = true;
        _state.timerPaused = false;
        _lastTick = millis();

        DEBUG_PRINTF("[Timer] Absolute timer set for %02d:%02d\n", hour, minute);
        bus().publish(EventType::TimerStarted, (int)_state.timerRemaining);
        return true;
    }

    // Get timer mode
    TimerMode getMode() const { 
        return (TimerMode)_state.timerMode; 
    }

    // Get alarm time (for absolute mode)
    void getAlarmTime(uint8_t& hour, uint8_t& minute) const {
        hour = _state.timerAlarmHour;
        minute = _state.timerAlarmMinute;
    }

private:
    MachineState& _state;
    RTCPlugin* _rtc = nullptr;
    uint32_t _lastTick = 0;

    // Update remaining time for absolute timer based on current RTC time
    bool updateAbsoluteRemaining();

    void complete() {
        _state.timerActive = false;
        _state.timerPaused = false;

        DEBUG_PRINTLN("[Timer] Completed!");
        bus().publish(EventType::TimerCompleted);
    }
};

// ─── Implementation requiring RTCPlugin ─────────────────────
// Include RTCPlugin header for implementation
#include "RTCPlugin.h"

inline bool TimerPlugin::updateAbsoluteRemaining() {
    if (!_rtc || !_rtc->isAvailable()) {
        return false;
    }

    // Get current time from RTC
    DateTime now = _rtc->getDateTime();
    uint32_t currentSec = now.hour() * 3600 + now.minute() * 60 + now.second();
    uint32_t targetSec = _state.timerAlarmHour * 3600 + _state.timerAlarmMinute * 60;

    // If target time is in the past (today), it means tomorrow
    if (targetSec <= currentSec) {
        // Add 24 hours
        targetSec += 24 * 3600;
    }

    _state.timerRemaining = targetSec - currentSec;
    _state.timerTotal = _state.timerRemaining;
    
    return true;
}
