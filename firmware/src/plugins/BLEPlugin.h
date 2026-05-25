#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#ifdef SIM_BUILD
#include <cstdio>
#endif
#include "../core/Plugin.h"
#include "../core/constants.h"
#include "../models/MachineState.h"
#include "../protocol/protocol.h"

class BLEPlugin : public Plugin,
                  public NimBLECharacteristicCallbacks,
                  public NimBLEServerCallbacks {
public:
    const char* getName() const override { return "BLE"; }

    bool setup() override {
        NimBLEDevice::init(BLE_DEVICE_NAME);
        NimBLEDevice::setMTU(
#ifdef BLE_MTU_SIZE
            BLE_MTU_SIZE
#else
            512
#endif
        );

        _server = NimBLEDevice::createServer();
        _server->setCallbacks(this);

        // Nordic UART Service
        NimBLEService* service = _server->createService(BLE_SERVICE_UUID);

        // TX characteristic (notify: device → app)
        _txChar = service->createCharacteristic(
            BLE_CHAR_TX_UUID,
            NIMBLE_PROPERTY::NOTIFY
        );

        // RX characteristic (write: app → device)
        _rxChar = service->createCharacteristic(
            BLE_CHAR_RX_UUID,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
        );
        _rxChar->setCallbacks(this);

        service->start();

        // Advertising
        NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
        advertising->addServiceUUID(BLE_SERVICE_UUID);
        advertising->setName(BLE_DEVICE_NAME);
        advertising->start();

        // Subscribe to BLESend events from other plugins
        bus().subscribe(EventType::BLESend, [this](const Event& e) {
            send(e.stringValue);
        });

        // Recipe paused on WAIT_CONFIRM — notify the app so it can prompt the user.
        // Telemetry also carries wc/cm so the modal can re-open after a reconnect.
        bus().subscribe(EventType::RecipeWaitConfirm, [this](const Event& e) {
            JsonDocument doc;
            doc[Protocol::FIELD_TYPE] = Protocol::EVT_RECIPE_CONFIRM;
            doc["msg"] = e.stringValue;
            sendJson(doc);
        });

        DEBUG_PRINTLN("[BLE] Service started, advertising...");
        return true;
    }

    void loop() override {
        // Drain any BLE commands queued by onWrite. Doing this here
        // instead of inside onWrite means the synchronous handlers
        // (SD reads, JSON serialization, notify bursts) run on the
        // main loop task, not on the NimBLE host task. The host
        // task being blocked for >300 ms is what was tripping the
        // BLE supervision timer and disconnecting on req:recipe:load.
        //
        // Drain up to CMD_QUEUE_DEPTH per tick so a burst (e.g. the
        // app firing req:settings:thermal:get + req:settings:cal:get
        // on connect) clears in one pass instead of trickling out one
        // command per loop iteration.
        String cmd;
        while (popCommand(cmd)) {
            bus().publish(EventType::BLECommandReceived, cmd);
        }

        // Send telemetry periodically
        uint32_t now = millis();
        if (_connected && (now - _lastTelemetry >= TELEMETRY_INTERVAL_MS)) {
            _lastTelemetry = now;
            sendTelemetry();
        }
    }

    // ── Send Data to App ────────────────────────────────────

    void send(const String& json) {
#ifdef SIM_BUILD
        // Simulator path: emit one JSON line per call to stdout. The Node
        // bridge (tools/sim-bridge/server.js) forwards it to the connected
        // WebSocket client. CI consumers can pipe directly.
        std::fputs(json.c_str(), stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
        return;
#else
        sendBytes(reinterpret_cast<const uint8_t*>(json.c_str()), json.length());
#endif
    }

#ifndef SIM_BUILD
    // Returns the MTU that the *peer* has agreed to on the current
    // link. NimBLEDevice::getMTU() returns the locally configured
    // preferred MTU (185 on this build) regardless of what was
    // actually negotiated, so chunking by it caused chunks larger
    // than the peer would accept whenever the central hadn't yet
    // completed the MTU exchange (BLE default = 23). The controller
    // silently drops the link in that case — symptom matches the
    // "BLE desconectou ao abrir uma receita" report.
    uint16_t getNegotiatedMtu() const {
        if (_server && _connHandle != BLE_HS_CONN_HANDLE_NONE) {
            uint16_t mtu = _server->getPeerMTU(_connHandle);
            if (mtu > 0) return mtu;
        }
        // BLE default ATT MTU is 23. Stay conservative pre-exchange.
        return 23;
    }

    // Core BLE chunked-notify path. Earlier versions dropped chunks
    // silently when NimBLE's TX mbuf pool ran out on a large burst
    // (loadRecipe / brewlog export ~4 KB payloads at MTU=185). The
    // dropped chunk left the central waiting indefinitely, and the
    // supervision timer eventually killed the link — surfaced in the
    // app as "BLE desconectou ao abrir uma receita".
    //
    // Fixes here, all in one place so every notify path shares them:
    //
    //   * check `notify()`'s bool return,
    //   * on failure, back off (5 / 10 / 20 ms) and retry up to 3x,
    //   * re-check `_connected`/`_txChar` between chunks so a
    //     mid-stream disconnect aborts cleanly,
    //   * keep the 2 ms per-chunk yield on the success path so the
    //     NimBLE host task isn't pinned long enough to trip the
    //     supervision timer.
    bool sendBytes(const uint8_t* data, size_t len) {
        if (!_connected || !_txChar || len == 0) return false;

        const size_t mtu = getNegotiatedMtu() - 3;  // 3 bytes ATT overhead

        for (size_t i = 0; i < len; i += mtu) {
            if (!_connected || !_txChar) return false;
            const size_t chunk = (len - i < mtu) ? (len - i) : mtu;
            bool sent = false;
            for (uint8_t attempt = 0; attempt < 3 && _connected; ++attempt) {
                _txChar->setValue(data + i, chunk);
                if (_txChar->notify()) { sent = true; break; }
                // Notify queue full or transient ENOMEM — back off
                // and retry. Backoffs stay short enough not to trip
                // the supervision timer but long enough for the host
                // task to drain its TX mbuf pool.
                delay(5 << attempt);  // 5 / 10 / 20 ms
            }
            if (!sent) {
                DEBUG_PRINTLN("[BLE] notify dropped chunk after 3 retries");
                return false;
            }
            if (len > mtu) delay(2);  // brief yield between chunks
        }
        return true;
    }
#endif

    void sendJson(JsonDocument& doc) {
#ifdef SIM_BUILD
        // Reuse a single String buffer to avoid per-call heap churn
        // on the sim stdout bridge. Cleared rather than freed so
        // capacity is retained.
        _telemetryBuf = "";
        serializeJson(doc, _telemetryBuf);
        send(_telemetryBuf);
#else
        if (!_connected || !_txChar) return;
        // Stream the JSON directly into BLE notify chunks via a
        // Print-derived adapter (defined below). Avoids the previous
        // `String json` intermediate, which held the full payload —
        // ~4 KB for a recipe load — and was the third copy of the
        // content along with the SD readString() result and the
        // JsonDocument internal storage. Triple-copy on a heap that
        // also has NimBLE + WiFi controllers + lwIP was tight enough
        // to occasionally fail malloc inside ArduinoJson's
        // serializer, which on ESP-IDF surfaces as a crash → reset →
        // "BLE desconectou" in the app.
        // getNegotiatedMtu() falls back to 23 (BLE default ATT MTU)
        // until the central completes the MTU exchange, so we never
        // send a chunk larger than the peer actually agreed to.
        ChunkedBleStream stream(_txChar, &_connected,
                                getNegotiatedMtu() - 3);
        serializeJson(doc, stream);
        stream.flushTail();
#endif
    }

#ifndef SIM_BUILD
    // ArduinoJson serializeJson() drives anything that implements the
    // Arduino Print interface: write(uint8_t) + write(buf, n). This
    // adapter buffers up to one MTU worth of payload and notifies in
    // place, with the same retry-on-ENOMEM behaviour the legacy
    // sendBytes() path now has.
    class ChunkedBleStream : public Print {
    public:
        ChunkedBleStream(NimBLECharacteristic* tx, volatile bool* connected,
                         size_t mtu)
            : _tx(tx), _connected(connected),
              _mtu(mtu > sizeof(_buf) ? sizeof(_buf) : mtu) {}

        size_t write(uint8_t c) override { return write(&c, 1); }

        size_t write(const uint8_t* data, size_t n) override {
            if (_fail) return 0;
            size_t out = 0;
            while (n > 0 && *_connected) {
                size_t room = _mtu - _pos;
                size_t take = (n < room) ? n : room;
                memcpy(_buf + _pos, data, take);
                _pos  += take;
                data  += take;
                n     -= take;
                out   += take;
                if (_pos == _mtu) flush();
            }
            return out;
        }

        bool flushTail() { if (_pos > 0) flush(); return !_fail; }

    private:
        NimBLECharacteristic* _tx;
        volatile bool*        _connected;
        size_t                _mtu;
        // Sized to the typical MTU ceiling (185) plus headroom — both
        // S3 and C3 envs pin CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU=185.
        uint8_t               _buf[192];
        size_t                _pos  = 0;
        bool                  _fail = false;

        void flush() {
            if (!*_connected) { _fail = true; _pos = 0; return; }
            for (uint8_t attempt = 0; attempt < 3 && *_connected; ++attempt) {
                _tx->setValue(_buf, _pos);
                if (_tx->notify()) { _pos = 0; delay(2); return; }
                delay(5 << attempt);  // 5 / 10 / 20 ms
            }
            _fail = true;
            _pos = 0;
            DEBUG_PRINTLN("[BLE] notify chunk dropped after 3 retries");
        }
    };
#endif

    bool isConnected() const { return _connected; }

    // ── Callback: Send event to app ─────────────────────────

    void sendEvent(const char* type, JsonDocument& doc) {
        doc[Protocol::FIELD_TYPE] = type;
        sendJson(doc);
    }

    void sendResponse(const char* type, const String& rid, JsonDocument& doc) {
        doc[Protocol::FIELD_TYPE] = type;
        doc[Protocol::FIELD_REQUEST_ID] = rid;
        sendJson(doc);
    }

    void sendError(const String& rid, const String& error) {
        JsonDocument doc;
        doc[Protocol::FIELD_TYPE] = Protocol::RES_ERROR;
        doc[Protocol::FIELD_REQUEST_ID] = rid;
        doc[Protocol::FIELD_ERROR] = error;
        sendJson(doc);
    }

private:
    NimBLEServer* _server = nullptr;
    NimBLECharacteristic* _txChar = nullptr;
    NimBLECharacteristic* _rxChar = nullptr;
    // Tracked from onConnect/onDisconnect so we can query the
    // per-connection negotiated MTU before each notify burst.
    uint16_t _connHandle = BLE_HS_CONN_HANDLE_NONE;
#ifdef SIM_BUILD
    // Simulator runs as if a client is permanently connected so telemetry
    // streams without a connection handshake.
    bool _connected = true;
#else
    bool _connected = false;
#endif
    uint32_t _lastTelemetry = 0;
    String _rxBuffer = "";
    // Reusable JsonDocument for telemetry — avoids ~12 heap allocs/sec at 1Hz
    // telemetry. clear() is called at the start of every sendTelemetry().
    JsonDocument _telemetryDoc;
    String _telemetryBuf;

    // Deferred command dispatch — populated from the NimBLE host
    // task (onWrite), drained from the main task (loop). SPSC ring
    // with portMUX critical sections on hardware: previously a single
    // slot dropped any second command that arrived between two main
    // loop ticks (e.g. app firing thermal:get + cal:get on connect).
    static constexpr size_t CMD_QUEUE_DEPTH = 8;
    String           _cmdQueue[CMD_QUEUE_DEPTH];
    uint8_t          _cmdHead = 0;
    uint8_t          _cmdTail = 0;
    uint8_t          _cmdCount = 0;
#ifndef SIM_BUILD
    portMUX_TYPE     _cmdMux = portMUX_INITIALIZER_UNLOCKED;
#endif

    bool pushCommand(String&& raw) {
#ifndef SIM_BUILD
        portENTER_CRITICAL(&_cmdMux);
#endif
        bool full = (_cmdCount >= CMD_QUEUE_DEPTH);
        if (!full) {
            _cmdQueue[_cmdHead] = std::move(raw);
            _cmdHead = (_cmdHead + 1) % CMD_QUEUE_DEPTH;
            _cmdCount++;
        }
#ifndef SIM_BUILD
        portEXIT_CRITICAL(&_cmdMux);
#endif
        return !full;
    }

    bool popCommand(String& out) {
#ifndef SIM_BUILD
        portENTER_CRITICAL(&_cmdMux);
#endif
        bool empty = (_cmdCount == 0);
        if (!empty) {
            out = std::move(_cmdQueue[_cmdTail]);
            _cmdQueue[_cmdTail] = String();
            _cmdTail = (_cmdTail + 1) % CMD_QUEUE_DEPTH;
            _cmdCount--;
        }
#ifndef SIM_BUILD
        portEXIT_CRITICAL(&_cmdMux);
#endif
        return !empty;
    }

    // ── NimBLE Server Callbacks ─────────────────────────────

    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        _connected = true;
        // Capture the connection handle so sendJson/sendBytes can ask
        // NimBLE for the *negotiated* MTU on this specific link, not
        // the configured preferred MTU returned by NimBLEDevice::getMTU.
        // Until the central completes the MTU exchange the link is on
        // the BLE default of 23; sending 182-byte chunks against a
        // peer that hasn't agreed to >23 makes the controller drop
        // the link with no log on either side.
        _connHandle = connInfo.getConnHandle();
        gState.bleConnected = true;
        bus().publish(EventType::BLEClientConnected);
        DEBUG_PRINTF("[BLE] Client connected: %s (conn=%u)\n",
                      connInfo.getAddress().toString().c_str(), _connHandle);

        // Allow multiple connections
        NimBLEDevice::getAdvertising()->start();

        // Notify app if recovery data is available
        if (gState.hasRecoveryData) {
            delay(500);  // Give app time to set up
            sendRecoveryNotification();
        }
    }
    
    void sendRecoveryNotification() {
        JsonDocument doc;
        doc[Protocol::FIELD_TYPE] = Protocol::EVT_RECIPE_RECOVERY;
        doc["recipe"] = (const char*)gState.recoveryRecipeName;
        sendJson(doc);
        DEBUG_PRINTF("[BLE] Sent recovery notification: %s\n",
                     gState.recoveryRecipeName);
    }

    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
        _connected = (server->getConnectedCount() > 0);
        if (!_connected) _connHandle = BLE_HS_CONN_HANDLE_NONE;
        gState.bleConnected = _connected;
        if (!_connected) {
            bus().publish(EventType::BLEClientDisconnected);
        }
        DEBUG_PRINTF("[BLE] Client disconnected (reason=%d)\n", reason);
        NimBLEDevice::getAdvertising()->start();
    }

    // ── NimBLE Characteristic Callbacks ─────────────────────

    void onWrite(NimBLECharacteristic* characteristic, NimBLEConnInfo& connInfo) override {
        std::string value = characteristic->getValue();
        if (value.empty()) return;

        // Accumulate chunks into buffer
        _rxBuffer += String(value.c_str());

        // Try to parse as JSON (might be incomplete if chunked)
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, _rxBuffer);
        
        if (err == DeserializationError::Ok) {
            _rxBuffer = "";  // Reset buffer
            handleCommand(doc);
        } else if (err != DeserializationError::IncompleteInput) {
            // Bad JSON, reset
            DEBUG_PRINTF("[BLE] JSON parse error: %s\n", err.c_str());
            _rxBuffer = "";
        }
        // If IncompleteInput, keep accumulating
    }

    // ── Command Handler ─────────────────────────────────────
    //
    // Called from onWrite on the NimBLE host task. We DO NOT run the
    // real handler here — synchronous SD reads + JSON serialize +
    // notify bursts can keep the host task pinned past the BLE
    // supervision timeout. Instead we capture the raw JSON and let
    // loop() (main task) publish it on the EventBus where the actual
    // handlers can take their time without killing the link.
    void handleCommand(JsonDocument& doc) {
        const char* type = doc[Protocol::FIELD_TYPE] | "";
        DEBUG_PRINTF("[BLE] Command: %s\n", type);

        String raw;
        serializeJson(doc, raw);
        if (!pushCommand(std::move(raw))) {
            // Ring full — drop newest. Depth is CMD_QUEUE_DEPTH; if we
            // overflow that, the consumer side is stuck and dropping is
            // the lesser evil vs. blocking the NimBLE host task.
            DEBUG_PRINTLN("[BLE] command dropped: queue full");
        }
    }

    // ── Telemetry ───────────────────────────────────────────

    void sendTelemetry() {
        JsonDocument& doc = _telemetryDoc;
        doc.clear();
        doc[Protocol::FIELD_TYPE]         = Protocol::EVT_STATUS;
        doc[Protocol::FIELD_CURRENT_TEMP] = round2(gState.currentTemp);
        doc["adc"]                        = gState.ntcAdcLast;
        doc["tso"]                        = gState.tempSensorOk;
        doc[Protocol::FIELD_TARGET_TEMP]  = round2(gState.targetTemp);
        doc[Protocol::FIELD_PID_OUTPUT]   = (int)gState.pidOutput;
        doc[Protocol::FIELD_HEATER_ON]    = gState.heaterOn;
        doc[Protocol::FIELD_PUMP_ON]      = gState.pumpOn;
        doc[Protocol::FIELD_UPTIME]       = millis() / 1000;

        // Mode
        switch (gState.mode) {
            case OperatingMode::Manual:  doc[Protocol::FIELD_MODE] = Protocol::MODE_MANUAL; break;
            case OperatingMode::Recipe:  doc[Protocol::FIELD_MODE] = Protocol::MODE_RECIPE; break;
            case OperatingMode::Tuning:  doc[Protocol::FIELD_MODE] = Protocol::MODE_TUNING; break;
            default:                     doc[Protocol::FIELD_MODE] = Protocol::MODE_IDLE; break;
        }

        // Recipe info (when running). `rst` exposes the engine sub-state
        // so the app can swap dedicated views (WaitTemp / WaitTimer /
        // WaitConfirm / Paused) without inferring from other fields.
        if (gState.mode == OperatingMode::Recipe) {
            doc[Protocol::FIELD_RECIPE_STEP]  = gState.recipeStep;
            doc[Protocol::FIELD_RECIPE_TOTAL] = gState.recipeTotalSteps;
            doc[Protocol::FIELD_RECIPE_NAME]  = (const char*)gState.recipeName;
            doc[Protocol::FIELD_TIMER_LEFT]   = gState.timerRemainingMs / 1000;
            const char* rst = "running";
            switch (gState.recipeState) {
                case RecipeState::Idle:                  rst = "idle"; break;
                case RecipeState::Running:               rst = "running"; break;
                case RecipeState::Paused:                rst = "paused"; break;
                case RecipeState::WaitingForTemperature: rst = "waiting_temp"; break;
                case RecipeState::WaitingForTimer:       rst = "waiting_timer"; break;
                case RecipeState::Preparing:             rst = "running"; break;
                case RecipeState::WaitingForConfirm:     rst = "waiting_confirm"; break;
                case RecipeState::Completed:             rst = "completed"; break;
            }
            doc["rst"] = rst;
        }

        // Ramp info
        if (gState.rampActive) {
            doc["ra"]  = true;
            doc["rr"]  = round2(gState.rampRate);
            doc["rtg"] = round2(gState.rampTarget);
            doc["rc"]  = round2(gState.rampCurrent);
        }

        // Brew Log info
        if (gState.brewLogActive) {
            doc["bla"] = true;
            doc["ble"] = gState.brewLogEntries;
        }

        // Boil Timer info
        if (gState.boilActive) {
            doc["ba"]  = true;
            doc["bt"]  = gState.boilTotal;
            doc["br"]  = gState.boilRemaining;
            doc["bad"] = gState.boilAdditions;
        }

        // Mash-Out info
        if (gState.mashOutEnabled) {
            doc["moe"] = true;
            doc["mot"] = round2(gState.mashOutTemp);
        }

        // RTC info
        if (gState.rtcAvailable) {
            doc["rtca"] = true;
            doc["rtct"] = gState.rtcTimestamp;
            doc["rtcn"] = gState.rtcNtpSynced;
        }

        // Timer info (generic countdown timer)
        // P1 fix: tmrP/tmrT keys avoid collision with FIELD_TYPE("tp") and
        // FIELD_TARGET_TEMP("tt") — old keys froze the UI when timer was active.
        // Always emit ta so the UI can reset when the timer completes (without
        // it the client would never see ta flip back to false).
        doc["ta"] = gState.timerActive;
        if (gState.timerActive) {
            doc["tmrP"] = gState.timerPaused;
            doc["tmrT"] = gState.timerTotal;
            doc["tr"]   = gState.timerRemaining;
            doc["tm"]   = gState.timerMode;  // 0=relative, 1=absolute
            if (gState.timerMode == 1) {     // Absolute mode
                doc["tah"] = gState.timerAlarmHour;
                doc["tam"] = gState.timerAlarmMinute;
            }
        }

        // Scheduler info ("be ready at HH:MM"). Always emit `sa` so the
        // app can detect the inactive→active and active→inactive edges
        // — otherwise canceling never clears the UI because we'd just
        // stop sending the fields and the cached signals would stay true.
        doc["sa"] = gState.schedulerActive;
        if (gState.schedulerActive) {
            doc["sth"] = gState.schedulerTargetHour;
            doc["stm"] = gState.schedulerTargetMinute;
            doc["stt"] = gState.schedulerTargetTemp;
            doc["sv"] = gState.schedulerVolume;
            doc["ss"] = (const char*)gState.schedulerStatus;
        }

        // Auto-Tune info
        if (gState.autoTuneActive) {
            doc["ata"] = true;
            doc["atp"] = gState.autoTuneProgress;
        }

        // Brewing step (for UI)
        if (gState.brewingStep != BrewingStep::None || !isEmptyStr(gState.brewingStepCustom)) {
            doc["bs"] = (int)gState.brewingStep;
            if (!isEmptyStr(gState.brewingStepCustom)) {
                doc["bsc"] = (const char*)gState.brewingStepCustom;
            }
        }

        // WAIT_CONFIRM state — always emitted so the UI clears the modal once
        // the recipe advances past the wait.
        doc["wc"] = gState.waitingForConfirm;
        if (gState.waitingForConfirm && !isEmptyStr(gState.confirmMessage)) {
            doc["cm"] = (const char*)gState.confirmMessage;
        }

        // Thermal Watchdog status (001-thermal-watchdog T024). Nested
        // object key avoids short-key collisions documented in DT-01.
        // Always emitted so the app can render an indicator and detect
        // firmware that supports the feature.
        auto wd = doc[Protocol::FIELD_WATCHDOG].to<JsonObject>();
        wd[Protocol::WD_FIELD_ARMED]      = gState.watchdogArmed;
        wd[Protocol::WD_FIELD_TRIPPED]    = gState.watchdogTripped;
        wd[Protocol::WD_FIELD_COUNT]      = gState.watchdogTripCount;
        if (gState.watchdogLastCause != WatchdogCause::NONE) {
            wd[Protocol::WD_FIELD_LAST_CAUSE] = getWatchdogCauseName(gState.watchdogLastCause);
            wd[Protocol::WD_FIELD_LAST_UNIX]  = gState.watchdogLastTripUnix;
        }
        wd[Protocol::WD_FIELD_HARD_STOP]        = round2(gState.watchdogHardStopC);
        wd[Protocol::WD_FIELD_AUTO_RESET]       = gState.watchdogAutoResetEnabled;
        wd[Protocol::WD_FIELD_SENSOR_FAULT_MS]  = gState.watchdogSensorFaultMs;
        wd[Protocol::WD_FIELD_LOOP_STUCK_MS]    = gState.watchdogLoopStuckMs;
        wd[Protocol::WD_FIELD_GRAD_FACTOR]      = gState.watchdogGradFactor;
        wd[Protocol::WD_FIELD_GRAD_WINDOW]      = gState.watchdogGradWindow;
        wd[Protocol::WD_FIELD_SAFE_AUTORESET_C] = round2(gState.watchdogSafeAutoresetC);
        wd[Protocol::WD_FIELD_COOL_MIN_MS]      = gState.watchdogCoolMinMs;

        // Ambient (effective + raw source state) — UI uses these to render
        // the badge "Manual" / "Sensor (ok)" / "Sensor (stale → manual)".
        doc["ambEff"]      = round2(getEffectiveAmbient());
        doc["ambSrc"]      = gState.ambientSource == AmbientSource::SENSOR ? "sensor" : "manual";
        doc["ambSensorOk"] = gState.ambientSensorOk;
        doc["lidState"]    = gState.lidState == LidState::ON ? "lidOn" : "lidOff";

        // LossTune progress — emitted only when active so steady-state
        // status payload stays compact.
        if (gState.lossTuneActive
            || gState.lossTunePhase == LossTunePhase::RESULT
            || gState.lossTunePhase == LossTunePhase::ERROR) {
            auto lt = doc["lt"].to<JsonObject>();
            lt["ph"]   = getLossTunePhaseName(gState.lossTunePhase);
            lt["pct"]  = gState.lossTuneProgressPct;
            lt["mode"] = gState.lossTuneTargetMode == LidState::ON ? "lidOn" : "lidOff";
            lt["r2"]   = round2(gState.lossTuneR2);
            lt["h"]    = round2(gState.lossTuneFittedCoeff);
            lt["n"]    = gState.lossTuneSampleCount;
            if (gState.lossTuneError[0]) lt["err"] = gState.lossTuneError;
        }

        sendJson(doc);
    }

    float round2(float val) {
        return round(val * 100.0f) / 100.0f;
    }
};
