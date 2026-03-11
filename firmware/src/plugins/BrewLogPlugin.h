#pragma once

#include <Arduino.h>
#include <vector>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../models/MachineState.h"

// ─── Brew Log Entry ─────────────────────────────────────────
struct BrewLogEntry {
    uint32_t timestamp;   // ms since log start
    float    temp;        // current temperature
    float    target;      // target temperature
    float    output;      // PID output (0-255)
    bool     heater;      // heater state
    bool     pump;        // pump state
};

// ─── Brew Log Plugin ────────────────────────────────────────
// Records brewing session data for later export (CSV/JSON)

class BrewLogPlugin : public Plugin {
public:
    static constexpr uint16_t MAX_ENTRIES = 3600;  // ~1 hour at 1Hz
    static constexpr uint32_t LOG_INTERVAL_MS = 1000;

    const char* getName() const override { return "BrewLog"; }

    bool setup() override {
        DEBUG_PRINTLN("[BrewLog] Initialized");
        return true;
    }

    void loop() override {
        if (!_logging) return;

        uint32_t now = millis();
        if (now - _lastLog < LOG_INTERVAL_MS) return;
        _lastLog = now;

        // Create log entry
        BrewLogEntry entry;
        entry.timestamp = now - _startTime;
        entry.temp = gState.currentTemp;
        entry.target = gState.targetTemp;
        entry.output = gState.pidOutput;
        entry.heater = gState.heaterOn;
        entry.pump = gState.pumpOn;

        // Add to buffer (circular if full)
        if (_entries.size() >= MAX_ENTRIES) {
            _entries.erase(_entries.begin());
        }
        _entries.push_back(entry);

        gState.brewLogEntries = _entries.size();
    }

    void start() {
        if (_logging) return;
        
        _entries.clear();
        _startTime = millis();
        _lastLog = 0;
        _logging = true;
        
        gState.brewLogActive = true;
        gState.brewLogStartTime = _startTime;
        gState.brewLogEntries = 0;
        
        DEBUG_PRINTLN("[BrewLog] Started recording");
    }

    void stop() {
        _logging = false;
        gState.brewLogActive = false;
        DEBUG_PRINTF("[BrewLog] Stopped. %d entries recorded\n", _entries.size());
    }

    bool isLogging() const { return _logging; }
    uint16_t getEntryCount() const { return _entries.size(); }

    // Export as CSV string (chunked for BLE)
    String exportCSV(uint16_t startIdx = 0, uint16_t count = 50) {
        String csv;
        
        if (startIdx == 0) {
            csv = "time_s,temp,target,output,heater,pump\n";
        }
        
        uint16_t endIdx = min((uint16_t)_entries.size(), (uint16_t)(startIdx + count));
        for (uint16_t i = startIdx; i < endIdx; i++) {
            const auto& e = _entries[i];
            csv += String(e.timestamp / 1000.0f, 1) + ",";
            csv += String(e.temp, 2) + ",";
            csv += String(e.target, 2) + ",";
            csv += String(e.output, 1) + ",";
            csv += String(e.heater ? 1 : 0) + ",";
            csv += String(e.pump ? 1 : 0) + "\n";
        }
        
        return csv;
    }

    // Export as JSON string (chunked for BLE)
    String exportJSON(uint16_t startIdx = 0, uint16_t count = 30) {
        String json;
        
        if (startIdx == 0) {
            json = "{\"entries\":[";
        }
        
        uint16_t endIdx = min((uint16_t)_entries.size(), (uint16_t)(startIdx + count));
        for (uint16_t i = startIdx; i < endIdx; i++) {
            const auto& e = _entries[i];
            if (i > startIdx || startIdx > 0) json += ",";
            json += "{\"t\":" + String(e.timestamp / 1000.0f, 1);
            json += ",\"c\":" + String(e.temp, 2);
            json += ",\"s\":" + String(e.target, 2);
            json += ",\"o\":" + String(e.output, 1);
            json += ",\"h\":" + String(e.heater ? 1 : 0);
            json += ",\"p\":" + String(e.pump ? 1 : 0) + "}";
        }
        
        // Close array if last chunk
        if (endIdx >= _entries.size()) {
            json += "]}";
        }
        
        return json;
    }

    // Get export chunk info
    uint16_t getTotalChunks(uint16_t chunkSize) const {
        if (_entries.empty()) return 0;
        return (_entries.size() + chunkSize - 1) / chunkSize;
    }

private:
    bool _logging = false;
    uint32_t _startTime = 0;
    uint32_t _lastLog = 0;
    std::vector<BrewLogEntry> _entries;
};
