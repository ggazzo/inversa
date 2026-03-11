#pragma once

#include <Arduino.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/constants.h"
#include "../models/MachineState.h"

// ─── Timer Plugin ───────────────────────────────────────────
// Generic countdown timer for brewing operations.
// - Relative timers (duration-based)
// - Start, stop, pause, resume functionality
// - Publishes tick events every second

class TimerPlugin : public Plugin {
public:
    TimerPlugin(MachineState& state) : _state(state) {}

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

            if (_state.timerRemaining > 0) {
                _state.timerRemaining--;
                bus().publish(EventType::TimerTick, (int)_state.timerRemaining);
            }

            if (_state.timerRemaining == 0) {
                complete();
            }
        }
    }

    // ── Public API ───────────────────────────────────────────

    // Start timer with duration in seconds
    void start(uint32_t durationSec) {
        if (durationSec == 0) {
            DEBUG_PRINTLN("[Timer] Cannot start timer with 0 duration");
            return;
        }

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

private:
    MachineState& _state;
    uint32_t _lastTick = 0;

    void complete() {
        _state.timerActive = false;
        _state.timerPaused = false;

        DEBUG_PRINTLN("[Timer] Completed!");
        bus().publish(EventType::TimerCompleted);
    }
};
