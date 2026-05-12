#pragma once

#include <Arduino.h>
#include <SD.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../models/MachineState.h"
#include "SDCardPlugin.h"

// ─── Brew Log Entry ─────────────────────────────────────────
// Packed so SD round-trips are deterministic across builds.
// Size: 4 + 4 + 4 + 4 + 1 + 1 = 18 bytes (with packing).
#pragma pack(push, 1)
struct BrewLogEntry {
    uint32_t timestamp;   // ms since log start
    float    temp;        // current temperature
    float    target;      // target temperature
    float    output;      // PID output (0-255)
    uint8_t  heater;      // 0/1
    uint8_t  pump;        // 0/1
};
#pragma pack(pop)

// ─── Brew Log Plugin (P18) ──────────────────────────────────
// Records brewing session data for later export (CSV/JSON).
//
// V2 design (2026-05):
//   - True O(1) ring buffer (fixed array + head/size). Previous version used
//     std::vector::erase(begin()) on overflow, an O(N) memmove on every tick
//     past entry 3600.
//   - SD persistence: every entry is buffered in RAM and flushed to
//     `/brewlog.bin` in batches of FLUSH_EVERY entries. A power-cut loses at
//     most the last batch (~30s). On stop() the buffer is force-flushed.
//   - Recovery: if a session was already on disk when start() is called, the
//     previous file is truncated. Re-opening past sessions is out of scope —
//     this is "current session, survives reboot until next start()".

class BrewLogPlugin : public Plugin {
public:
    static constexpr uint16_t MAX_ENTRIES   = 3600;            // ~1 hour @ 1Hz
    static constexpr uint32_t LOG_INTERVAL_MS = 1000;
    static constexpr uint16_t FLUSH_EVERY   = 30;              // batch flush
    static constexpr const char* LOG_PATH   = "/brewlog.bin";

    const char* getName() const override { return "BrewLog"; }

    // Called from main.cpp after PluginManager::add().
    void setSDCard(SDCardPlugin* sd) { _sd = sd; }

    bool setup() override {
        DEBUG_PRINTLN("[BrewLog] Initialized");
        return true;
    }

    void loop() override {
        if (!_logging) return;

        uint32_t now = millis();
        if (now - _lastLog < LOG_INTERVAL_MS) return;
        _lastLog = now;

        // ── Append to ring (O(1)) ────────────────────────────
        BrewLogEntry& slot = _ring[_head];
        slot.timestamp = now - _startTime;
        slot.temp      = gState.currentTemp;
        slot.target    = gState.targetTemp;
        slot.output    = gState.pidOutput;
        slot.heater    = gState.heaterOn ? 1 : 0;
        slot.pump      = gState.pumpOn   ? 1 : 0;

        _head = (_head + 1) % MAX_ENTRIES;
        if (_size < MAX_ENTRIES) _size++;
        // else: oldest entry overwritten; tail advances implicitly via _head.

        gState.brewLogEntries = _size;
        _pendingFlush++;

        if (_pendingFlush >= FLUSH_EVERY) {
            flushToSD();
        }
    }

    void start() {
        if (_logging) return;

        _head = _size = _pendingFlush = 0;
        _startTime = millis();
        _lastLog = 0;
        _logging = true;

        // Truncate prior session on SD (FILE_WRITE opens at 0 and overwrites).
        if (_sd && _sd->isMounted()) {
            File f = SD.open(LOG_PATH, FILE_WRITE);
            if (f) {
                f.close();
                DEBUG_PRINTF("[BrewLog] Truncated %s for new session\n", LOG_PATH);
            }
        }

        gState.brewLogActive    = true;
        gState.brewLogStartTime = _startTime;
        gState.brewLogEntries   = 0;

        DEBUG_PRINTLN("[BrewLog] Started recording");
    }

    void stop() {
        if (!_logging) return;
        flushToSD();  // P18 — never leave a tail buffered in RAM only.
        _logging = false;
        gState.brewLogActive = false;
        DEBUG_PRINTF("[BrewLog] Stopped. %u entries recorded\n", (unsigned)_size);
    }

    bool     isLogging()     const { return _logging; }
    uint16_t getEntryCount() const { return _size; }

    // ── Export ──────────────────────────────────────────────
    // Entries are streamed in CHRONOLOGICAL order regardless of ring wrap.
    // `startIdx` is the logical (0-based, oldest-first) offset.

    String exportCSV(uint16_t startIdx = 0, uint16_t count = 50) {
        String csv;
        if (startIdx == 0) {
            csv = "time_s,temp,target,output,heater,pump\n";
        }
        uint16_t endIdx = min((uint16_t)_size, (uint16_t)(startIdx + count));
        for (uint16_t i = startIdx; i < endIdx; i++) {
            const BrewLogEntry& e = at(i);
            csv += String(e.timestamp / 1000.0f, 1) + ",";
            csv += String(e.temp,   2) + ",";
            csv += String(e.target, 2) + ",";
            csv += String(e.output, 1) + ",";
            csv += String(e.heater ? 1 : 0) + ",";
            csv += String(e.pump   ? 1 : 0) + "\n";
        }
        return csv;
    }

    String exportJSON(uint16_t startIdx = 0, uint16_t count = 30) {
        String json;
        if (startIdx == 0) json = "{\"entries\":[";

        uint16_t endIdx = min((uint16_t)_size, (uint16_t)(startIdx + count));
        for (uint16_t i = startIdx; i < endIdx; i++) {
            const BrewLogEntry& e = at(i);
            if (i > startIdx || startIdx > 0) json += ",";
            json += "{\"t\":" + String(e.timestamp / 1000.0f, 1);
            json += ",\"c\":" + String(e.temp,   2);
            json += ",\"s\":" + String(e.target, 2);
            json += ",\"o\":" + String(e.output, 1);
            json += ",\"h\":" + String(e.heater ? 1 : 0);
            json += ",\"p\":" + String(e.pump   ? 1 : 0) + "}";
        }
        if (endIdx >= _size) json += "]}";
        return json;
    }

    uint16_t getTotalChunks(uint16_t chunkSize) const {
        if (_size == 0) return 0;
        return (_size + chunkSize - 1) / chunkSize;
    }

private:
    bool          _logging       = false;
    uint32_t      _startTime     = 0;
    uint32_t      _lastLog       = 0;
    BrewLogEntry  _ring[MAX_ENTRIES];
    uint16_t      _head          = 0;   // next write index
    uint16_t      _size          = 0;   // valid entries (≤ MAX_ENTRIES)
    uint16_t      _pendingFlush  = 0;   // entries appended since last flush
    SDCardPlugin* _sd            = nullptr;

    // Read i-th logical entry (oldest = 0) honoring ring wrap.
    const BrewLogEntry& at(uint16_t logicalIdx) const {
        uint16_t tail = (_size < MAX_ENTRIES) ? 0 : _head;
        return _ring[(tail + logicalIdx) % MAX_ENTRIES];
    }

    // P18 — append the last `_pendingFlush` ring entries to SD.
    // Each flush is one SD write (~600 bytes); ESP32 SD lib does ~10-50ms.
    void flushToSD() {
        if (_pendingFlush == 0) return;
        if (!_sd || !_sd->isMounted()) {
            _pendingFlush = 0;  // SD gone — keep going in RAM-only mode.
            return;
        }

        File f = SD.open(LOG_PATH, FILE_APPEND);
        if (!f) {
            DEBUG_PRINTLN("[BrewLog] flush: open failed");
            _pendingFlush = 0;
            return;
        }

        // The last `_pendingFlush` entries land at indices ending at _head-1,
        // walking backwards. Write them in chronological order.
        uint16_t startLogical = (_size >= _pendingFlush) ? (_size - _pendingFlush) : 0;
        for (uint16_t i = startLogical; i < _size; i++) {
            const BrewLogEntry& e = at(i);
            f.write(reinterpret_cast<const uint8_t*>(&e), sizeof(e));
        }
        f.close();
        _pendingFlush = 0;
    }
};
