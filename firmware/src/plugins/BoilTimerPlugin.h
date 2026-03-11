#pragma once

#include <Arduino.h>
#include <vector>
#include <algorithm>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../models/MachineState.h"

// ─── Hop/Addition Alert ─────────────────────────────────────
struct BoilAddition {
    uint16_t minutesRemaining;  // When to alert (minutes left in boil)
    char name[32];              // Description (e.g., "Cascade 60g")
    bool notified;              // Already sent notification
};

// ─── Boil Timer Plugin ──────────────────────────────────────
// Manages boil timer with hop addition alerts.
// Alerts are triggered based on time remaining (e.g., "60 min" = 60 min left).

class BoilTimerPlugin : public Plugin {
public:
    static constexpr uint8_t MAX_ADDITIONS = 10;
    static constexpr float DEFAULT_BOIL_TEMP = 100.0f;

    const char* getName() const override { return "BoilTimer"; }

    bool setup() override {
        DEBUG_PRINTLN("[BoilTimer] Initialized");
        return true;
    }

    void loop() override {
        if (!_running) return;

        uint32_t now = millis();
        uint32_t elapsed = now - _startTime - _pausedDuration;
        
        if (_paused) {
            _pausedDuration = now - _pauseStart;
            return;
        }

        // Update remaining time
        uint32_t elapsedSec = elapsed / 1000;
        if (elapsedSec >= _totalSeconds) {
            // Boil complete
            _running = false;
            gState.boilActive = false;
            gState.boilRemaining = 0;
            
            bus().publish(EventType::BoilCompleted);
            sendBoilEvent("complete");
            DEBUG_PRINTLN("[BoilTimer] Boil complete!");
            return;
        }

        uint32_t remaining = _totalSeconds - elapsedSec;
        gState.boilRemaining = remaining;

        // Check additions
        uint16_t remainingMin = remaining / 60;
        for (auto& add : _additions) {
            if (!add.notified && remainingMin <= add.minutesRemaining) {
                add.notified = true;
                sendAdditionAlert(add);
                DEBUG_PRINTF("[BoilTimer] Addition alert: %s @ %d min\n", 
                             add.name, add.minutesRemaining);
            }
        }
    }

    // Start boil timer
    void start(uint16_t minutes) {
        _totalSeconds = minutes * 60;
        _startTime = millis();
        _pausedDuration = 0;
        _running = true;
        _paused = false;
        
        // Reset addition notifications
        for (auto& add : _additions) {
            add.notified = false;
        }

        gState.boilActive = true;
        gState.boilTotal = _totalSeconds;
        gState.boilRemaining = _totalSeconds;

        // Set temperature to boil
        gState.targetTemp = DEFAULT_BOIL_TEMP;
        gState.mode = OperatingMode::Manual;
        bus().publish(EventType::SetpointChanged, DEFAULT_BOIL_TEMP);
        bus().publish(EventType::HeaterStateChanged, true);

        sendBoilEvent("started");
        DEBUG_PRINTF("[BoilTimer] Started %d min boil with %d additions\n", 
                     minutes, _additions.size());
    }

    void stop() {
        _running = false;
        _paused = false;
        gState.boilActive = false;
        sendBoilEvent("stopped");
        DEBUG_PRINTLN("[BoilTimer] Stopped");
    }

    void pause() {
        if (!_running || _paused) return;
        _paused = true;
        _pauseStart = millis();
        sendBoilEvent("paused");
        DEBUG_PRINTLN("[BoilTimer] Paused");
    }

    void resume() {
        if (!_running || !_paused) return;
        _pausedDuration += millis() - _pauseStart;
        _paused = false;
        sendBoilEvent("resumed");
        DEBUG_PRINTLN("[BoilTimer] Resumed");
    }

    // Add a hop/addition alert
    bool addAddition(uint16_t minutesRemaining, const char* name) {
        if (_additions.size() >= MAX_ADDITIONS) return false;
        
        BoilAddition add;
        add.minutesRemaining = minutesRemaining;
        strncpy(add.name, name, sizeof(add.name) - 1);
        add.name[sizeof(add.name) - 1] = '\0';
        add.notified = false;
        
        _additions.push_back(add);
        
        // Sort by time descending (largest first)
        std::sort(_additions.begin(), _additions.end(), 
            [](const BoilAddition& a, const BoilAddition& b) {
                return a.minutesRemaining > b.minutesRemaining;
            });
        
        gState.boilAdditions = _additions.size();
        DEBUG_PRINTF("[BoilTimer] Added: %s @ %d min\n", name, minutesRemaining);
        return true;
    }

    void clearAdditions() {
        _additions.clear();
        gState.boilAdditions = 0;
        DEBUG_PRINTLN("[BoilTimer] Cleared all additions");
    }

    bool isRunning() const { return _running; }
    bool isPaused() const { return _paused; }
    uint32_t getRemainingSeconds() const { return gState.boilRemaining; }
    const std::vector<BoilAddition>& getAdditions() const { return _additions; }

private:
    bool _running = false;
    bool _paused = false;
    uint32_t _startTime = 0;
    uint32_t _pauseStart = 0;
    uint32_t _pausedDuration = 0;
    uint32_t _totalSeconds = 0;
    std::vector<BoilAddition> _additions;

    void sendBoilEvent(const char* status) {
        String json = "{\"tp\":\"evt:boil:status\",\"st\":\"";
        json += status;
        json += "\",\"rem\":";
        json += String(gState.boilRemaining);
        json += ",\"total\":";
        json += String(gState.boilTotal);
        json += "}";
        bus().publish(EventType::BLESend, json);
    }

    void sendAdditionAlert(const BoilAddition& add) {
        String json = "{\"tp\":\"evt:boil:addition\",\"name\":\"";
        json += add.name;
        json += "\",\"min\":";
        json += String(add.minutesRemaining);
        json += "}";
        bus().publish(EventType::BLESend, json);
    }
};
