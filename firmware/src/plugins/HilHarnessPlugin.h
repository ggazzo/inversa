#pragma once

// HilHarnessPlugin — JSONL command channel for hardware-in-the-loop tests.
//
// Compiled only under HIL_BUILD. Lives on the same USB CDC the firmware
// already uses for debug output; the production debug stream is rewritten
// to prefix every line with `# ` (see DEBUG_PRINT* in core/constants.h)
// so the host bridge can demux logs from JSONL telemetry.
//
// Inbound commands (one JSON object per line):
//   {"cmd":"set","path":"ntc.c","value":<float>}        — drive NTC
//   {"cmd":"set","path":"ntc.bypassKalman","value":bool}— skip Kalman
//   {"cmd":"set","path":"ambient.c","value":<float>}    — drive ambient
//   {"cmd":"set","path":"ambient.ok","value":bool}      — flip sensor-ok
//   {"cmd":"clock","op":"advance","ms":<int>}           — +offset
//   {"cmd":"clock","op":"freeze"}                       — freeze now
//   {"cmd":"clock","op":"unfreeze"}                     — resume
//   {"cmd":"clock","op":"epoch","value":<unix_sec>}     — set virtual RTC
//   {"cmd":"force","path":"watchdog.trip","value":"OVERTEMP"|"LOOP_STUCK"|"PIN_STUCK"|"SENSOR_FAULT"|"GRADIENT"|"MANUAL"}
//   {"cmd":"get","path":"state"}                        — one-shot snapshot
//   {"cmd":"sub","topic":"state","hz":<int>}            — periodic state
//   {"cmd":"sub","topic":"events"|"ssr","enable":bool}  — toggle stream
//
// Outbound JSONL (timestamp `t` is the virtual clock):
//   {"event":"state","t":<vt>, …gState fields…}
//   {"event":"bus","t":<vt>,"type":"<EventType>","intValue":…}
//   {"event":"ssr","t":<vt>,"level":0|1}
//   {"event":"ack","t":<vt>,"cmd":"…","ok":true}
//   {"event":"err","t":<vt>,"msg":"…","raw":"…"}

#ifdef HIL_BUILD

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/constants.h"
#include "../models/MachineState.h"
#include "../hil/HilState.h"
#include "ThermalWatchdogPlugin.h"

class HilHarnessPlugin : public Plugin {
public:
    explicit HilHarnessPlugin(ThermalWatchdogPlugin* wd = nullptr)
        : _watchdog(wd) {}

    const char* getName() const override { return "HilHarness"; }

    bool setup() override {
        _lineBuf.reserve(256);
        // SSR baseline: capture initial level so the first emit is the
        // boot-time state (likely LOW after Watchdog::setup forces it).
        _lastSsrLevel = digitalRead(PIN_HEATER_SSR);
        // Subscribe to bus events so the harness can stream them. We
        // capture the wire-encoded enum value; the host bridge owns the
        // mapping back to a string name.
        bus().subscribe(EventType::WatchdogTripped,        [this](const Event& e){ onBus("WatchdogTripped",        e); });
        bus().subscribe(EventType::WatchdogReset,          [this](const Event& e){ onBus("WatchdogReset",          e); });
        bus().subscribe(EventType::TemperatureError,       [this](const Event& e){ onBus("TemperatureError",       e); });
        bus().subscribe(EventType::SystemReady,            [this](const Event& e){ onBus("SystemReady",            e); });
        bus().subscribe(EventType::HeaterStateChanged,     [this](const Event& e){ onBus("HeaterStateChanged",     e); });
        bus().subscribe(EventType::PIDOutputChanged,       [this](const Event& e){ onBus("PIDOutputChanged",       e); });
        bus().subscribe(EventType::SetpointChanged,        [this](const Event& e){ onBus("SetpointChanged",        e); });
        bus().subscribe(EventType::WatchdogConfigChanged,  [this](const Event& e){ onBus("WatchdogConfigChanged",  e); });
        bus().subscribe(EventType::WatchdogKick,           [this](const Event& e){ onBus("WatchdogKick",           e); });
        emitRaw("{\"event\":\"hello\",\"t\":%u,\"build\":\"hil_s3_mini\"}\n", millis());
        return true;
    }

    void loop() override {
        // 1. Drain inbound serial bytes, framing on newline.
        while (Serial.available()) {
            int c = Serial.read();
            if (c < 0) break;
            if (c == '\n' || c == '\r') {
                if (!_lineBuf.isEmpty()) {
                    handleLine(_lineBuf);
                    _lineBuf = "";
                }
            } else if (_lineBuf.length() < 1024) {
                _lineBuf += (char)c;
            } else {
                // Overflow guard: drop the rest of this line.
                _lineBuf = "";
            }
        }

        // 2. Periodic state emit.
        if (_stateHz > 0) {
            uint32_t periodMs = 1000u / (uint32_t)_stateHz;
            uint32_t now = millis();
            if (now - _lastStateMs >= periodMs) {
                _lastStateMs = now;
                emitState();
            }
        }

        // 3. SSR edge detection — polled every loop. The pin is the real
        //    safety signal; we don't trust gState.heaterOn alone.
        if (_ssrStream) {
            int lvl = digitalRead(PIN_HEATER_SSR);
            if (lvl != _lastSsrLevel) {
                _lastSsrLevel = lvl;
                emitRaw("{\"event\":\"ssr\",\"t\":%u,\"level\":%d}\n", millis(), lvl);
            }
        }
    }

private:
    void handleLine(const String& line) {
        // Tolerate optional whitespace / comment lines.
        if (line.length() == 0 || line[0] == '#') return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, line);
        if (err) {
            emitErr("parse_error", line.c_str());
            return;
        }
        const char* cmd = doc["cmd"] | "";

        if (!strcmp(cmd, "set")) {
            const char* path = doc["path"] | "";
            if (!strcmp(path, "ntc.c")) {
                hil::g_hil.ntcCelsius = doc["value"].as<float>();
                hil::g_hil.ntcActive  = true;
            } else if (!strcmp(path, "ntc.bypassKalman")) {
                hil::g_hil.ntcBypassKalman = doc["value"].as<bool>();
            } else if (!strcmp(path, "ntc.passthrough")) {
                // explicit "release the override" path
                hil::g_hil.ntcActive = false;
            } else if (!strcmp(path, "ambient.c")) {
                hil::g_hil.ambientCelsius = doc["value"].as<float>();
                hil::g_hil.ambientActive  = true;
                hil::g_hil.ambientOk      = true;
            } else if (!strcmp(path, "ambient.ok")) {
                hil::g_hil.ambientOk     = doc["value"].as<bool>();
                hil::g_hil.ambientActive = true;
            } else {
                emitErr("unknown_set_path", path);
                return;
            }
            ack(cmd);
            return;
        }

        if (!strcmp(cmd, "clock")) {
            const char* op = doc["op"] | "";
            if (!strcmp(op, "advance")) {
                int32_t add = doc["ms"].as<int32_t>();
                // Apply additively so successive advances accumulate. If
                // currently frozen, also bump freezeAt so unfreezing
                // resumes from the advanced point.
                hil::g_clock.offsetMs += add;
                if (hil::g_clock.frozen) hil::g_clock.freezeAt += add;
            } else if (!strcmp(op, "freeze")) {
                hil::g_clock.freezeAt = hil_clock_now();
                hil::g_clock.frozen   = true;
            } else if (!strcmp(op, "unfreeze")) {
                hil::g_clock.frozen = false;
            } else if (!strcmp(op, "epoch")) {
                hil::g_hil.rtcEpoch        = doc["value"].as<uint32_t>();
                hil::g_hil.rtcEpochSetAtMs = millis();
                hil::g_hil.rtcActive       = true;
            } else {
                emitErr("unknown_clock_op", op);
                return;
            }
            ack(cmd);
            return;
        }

        if (!strcmp(cmd, "force")) {
            const char* path = doc["path"] | "";
            if (!strcmp(path, "watchdog.trip")) {
                if (!_watchdog) { emitErr("no_watchdog", path); return; }
                const char* causeStr = doc["value"] | "";
                WatchdogCause c = WatchdogCause::MANUAL;
                if      (!strcmp(causeStr, "OVERTEMP"))     c = WatchdogCause::OVERTEMP;
                else if (!strcmp(causeStr, "LOOP_STUCK"))   c = WatchdogCause::LOOP_STUCK;
                else if (!strcmp(causeStr, "SENSOR_FAULT")) c = WatchdogCause::SENSOR_FAULT;
                else if (!strcmp(causeStr, "GRADIENT"))     c = WatchdogCause::GRADIENT;
                else if (!strcmp(causeStr, "PIN_STUCK"))    c = WatchdogCause::LOOP_STUCK;  // alias: brief uses both names
                else if (!strcmp(causeStr, "MANUAL"))       c = WatchdogCause::MANUAL;
                else { emitErr("unknown_watchdog_cause", causeStr); return; }
                _watchdog->hilForceTrip(c);
                ack(cmd);
                return;
            }
            emitErr("unknown_force_path", path);
            return;
        }

        if (!strcmp(cmd, "get")) {
            const char* path = doc["path"] | "";
            if (!strcmp(path, "state")) {
                emitState();
                return;
            }
            emitErr("unknown_get_path", path);
            return;
        }

        if (!strcmp(cmd, "sub")) {
            const char* topic = doc["topic"] | "";
            if (!strcmp(topic, "state")) {
                _stateHz = doc["hz"].as<int>();
                if (_stateHz < 0)  _stateHz = 0;
                if (_stateHz > 50) _stateHz = 50;
                _lastStateMs = 0;  // emit immediately on next loop
            } else if (!strcmp(topic, "events")) {
                _eventStream = doc["enable"] | true;
            } else if (!strcmp(topic, "ssr")) {
                _ssrStream = doc["enable"] | true;
                // Re-baseline so the first emit reflects current pin.
                _lastSsrLevel = digitalRead(PIN_HEATER_SSR);
            } else {
                emitErr("unknown_sub_topic", topic);
                return;
            }
            ack(cmd);
            return;
        }

        emitErr("unknown_cmd", cmd);
    }

    void emitState() {
        // Compact one-line JSON. Keys mirror the bridge-side TelemetryMsg
        // type. Float formatting uses %.3f everywhere to keep parses cheap.
        char buf[512];
        int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"state\",\"t\":%u,"
            "\"currentTemp\":%.3f,\"targetTemp\":%.3f,\"tempSensorOk\":%s,"
            "\"heaterOn\":%s,\"pidOutput\":%.3f,"
            "\"watchdogTripped\":%s,\"watchdogLastCause\":%u,\"watchdogTripCount\":%u,"
            "\"ambientSensorC\":%.3f,\"ambientSensorOk\":%s,"
            "\"mode\":%u,\"rtcAvailable\":%s,\"rtcTimestamp\":%u,"
            "\"ssr\":%d}\n",
            (unsigned)millis(),
            gState.currentTemp, gState.targetTemp, jb(gState.tempSensorOk),
            jb(gState.heaterOn), gState.pidOutput,
            jb(gState.watchdogTripped), (unsigned)gState.watchdogLastCause,
            (unsigned)gState.watchdogTripCount,
            gState.ambientSensorC, jb(gState.ambientSensorOk),
            (unsigned)gState.mode, jb(gState.rtcAvailable),
            (unsigned)gState.rtcTimestamp,
            digitalRead(PIN_HEATER_SSR));
        if (n > 0) Serial.write((const uint8_t*)buf, (size_t)n);
    }

    void onBus(const char* typeName, const Event& e) {
        if (!_eventStream) return;
        // Best-effort generic emit. Numeric union access depends on the
        // event family; we just publish all three views so the host can
        // pick whichever is meaningful.
        char buf[256];
        int n = snprintf(buf, sizeof(buf),
            "{\"event\":\"bus\",\"t\":%u,\"type\":\"%s\","
            "\"floatValue\":%.3f,\"intValue\":%d,\"boolValue\":%s}\n",
            (unsigned)millis(), typeName,
            e.floatValue, e.intValue, e.boolValue ? "true" : "false");
        if (n > 0) Serial.write((const uint8_t*)buf, (size_t)n);
    }

    void ack(const char* cmd) {
        emitRaw("{\"event\":\"ack\",\"t\":%u,\"cmd\":\"%s\",\"ok\":true}\n",
                (unsigned)millis(), cmd);
    }

    void emitErr(const char* msg, const char* raw) {
        // Quote the raw payload safely — only escape backslash and quote.
        char escaped[160];
        size_t j = 0;
        for (size_t i = 0; raw[i] && j < sizeof(escaped) - 2; ++i) {
            char c = raw[i];
            if (c == '\\' || c == '"') { escaped[j++] = '\\'; }
            escaped[j++] = c;
        }
        escaped[j] = 0;
        emitRaw("{\"event\":\"err\",\"t\":%u,\"msg\":\"%s\",\"raw\":\"%s\"}\n",
                (unsigned)millis(), msg, escaped);
    }

    void emitRaw(const char* fmt, ...) {
        char buf[320];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        if (n > 0) Serial.write((const uint8_t*)buf, (size_t)n);
    }

    static const char* jb(bool b) { return b ? "true" : "false"; }

    ThermalWatchdogPlugin* _watchdog;
    String   _lineBuf;
    int      _stateHz       = 0;
    uint32_t _lastStateMs   = 0;
    bool     _eventStream   = true;
    bool     _ssrStream     = true;
    int      _lastSsrLevel  = 0;
};

#endif  // HIL_BUILD
