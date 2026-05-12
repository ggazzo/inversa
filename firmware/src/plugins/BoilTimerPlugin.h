#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <algorithm>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/constants.h"
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
// Can be started via direct API or via EventBus (for recipe integration).

class BoilTimerPlugin : public Plugin {
public:
    static constexpr uint8_t MAX_ADDITIONS = 10;
    static constexpr float DEFAULT_BOIL_TEMP = 100.0f;

    const char* getName() const override { return "BoilTimer"; }

    bool setup() override {
        // Subscribe to BoilStarted event (from RecipePlugin)
        // Event format: floatValue = minutes, stringValue = JSON array of additions
        bus().subscribe(EventType::BoilStarted, [this](const Event& e) {
            uint16_t minutes = (uint16_t)e.floatValue;
            
            // Parse additions from JSON string
            if (!e.stringValue.isEmpty()) {
                parseAdditionsFromJson(e.stringValue);
            }
            
            // Start the boil
            start(minutes);
            DEBUG_PRINTF("[BoilTimer] Started via event: %d min\n", minutes);
        });

        DEBUG_PRINTLN("[BoilTimer] Initialized");
        return true;
    }

    void loop() override {
        if (!_running) return;

        // While paused: do nothing. _pausedDuration is accumulated atomically
        // in resume(). The previous loop body wrote `_pausedDuration = now -
        // _pauseStart` here, which combined with the `+=` in resume() caused a
        // double-count so the boil "lost" progress on every pause/resume cycle.
        if (_paused) return;

        uint32_t now = millis();
        uint32_t elapsed = now - _startTime - _pausedDuration;

        // Update remaining time
        uint32_t elapsedSec = elapsed / 1000;
        if (elapsedSec >= _totalSeconds) {
            // Boil complete
            _running = false;
            gState.boilActive    = false;
            gState.boilPaused    = false;
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
                // P16 — typed event with the addition name in stringValue so
                // RecipePlugin / brew log can react without parsing JSON.
                bus().publish(EventType::BoilAdditionAlert, String(add.name));
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

        gState.boilActive    = true;
        gState.boilPaused    = false;
        gState.boilTotal     = _totalSeconds;
        gState.boilRemaining = _totalSeconds;

        // Set temperature to boil
        gState.targetTemp = DEFAULT_BOIL_TEMP;
        // P17 — preserve mode if a higher-level controller is running (Recipe
        // calls us via EventBus::BoilStarted). Only force Manual when no one
        // else owns the heater.
        if (gState.mode == OperatingMode::Idle) {
            gState.mode = OperatingMode::Manual;
        }
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
        gState.boilPaused = false;
        sendBoilEvent("stopped");
        DEBUG_PRINTLN("[BoilTimer] Stopped");
    }

    void pause() {
        if (!_running || _paused) return;
        _paused = true;
        _pauseStart = millis();
        gState.boilPaused = true;
        sendBoilEvent("paused");
        // P16 — publish typed event so RecipePlugin (and other subscribers)
        // can react without parsing the BLE JSON envelope.
        bus().publish(EventType::BoilPaused);
        DEBUG_PRINTLN("[BoilTimer] Paused");
    }

    void resume() {
        if (!_running || !_paused) return;
        _pausedDuration += millis() - _pauseStart;
        _paused = false;
        gState.boilPaused = false;
        sendBoilEvent("resumed");
        bus().publish(EventType::BoilResumed);  // P16
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

    // Parse additions from JSON array string
    // Format: [{"min":60,"name":"Magnum"},{"min":15,"name":"Cascade"}]
    void parseAdditionsFromJson(const String& jsonStr) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, jsonStr);
        if (err) {
            DEBUG_PRINTF("[BoilTimer] Failed to parse additions JSON: %s\n", err.c_str());
            return;
        }

        clearAdditions();
        
        JsonArray arr = doc.as<JsonArray>();
        for (JsonObject obj : arr) {
            uint16_t min = obj["min"] | 0;
            const char* name = obj["name"] | "Addition";
            addAddition(min, name);
        }
        
        DEBUG_PRINTF("[BoilTimer] Parsed %d additions from JSON\n", _additions.size());
    }
};
