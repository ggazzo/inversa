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

        const size_t mtu = NimBLEDevice::getMTU() - 3;  // 3 bytes ATT overhead

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
        // Reuse a single String buffer to avoid per-call heap churn on the
        // BLE notify path. Cleared rather than freed so capacity is retained.
        _telemetryBuf = "";
        serializeJson(doc, _telemetryBuf);
        send(_telemetryBuf);
    }

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

    // ── NimBLE Server Callbacks ─────────────────────────────

    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        _connected = true;
        gState.bleConnected = true;
        bus().publish(EventType::BLEClientConnected);
        DEBUG_PRINTF("[BLE] Client connected: %s\n", 
                      connInfo.getAddress().toString().c_str());
        
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

    void handleCommand(JsonDocument& doc) {
        const char* type = doc[Protocol::FIELD_TYPE] | "";
        String rid = doc[Protocol::FIELD_REQUEST_ID] | "";

        DEBUG_PRINTF("[BLE] Command: %s\n", type);

        // Publish raw command for other plugins
        String raw;
        serializeJson(doc, raw);
        bus().publish(EventType::BLECommandReceived, raw);
    }

    // ── Telemetry ───────────────────────────────────────────

    void sendTelemetry() {
        JsonDocument& doc = _telemetryDoc;
        doc.clear();
        doc[Protocol::FIELD_TYPE]         = Protocol::EVT_STATUS;
        doc[Protocol::FIELD_CURRENT_TEMP] = round2(gState.currentTemp);
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

        // Scheduler info ("be ready at HH:MM")
        if (gState.schedulerActive) {
            doc["sa"] = true;
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

        sendJson(doc);
    }

    float round2(float val) {
        return round(val * 100.0f) / 100.0f;
    }
};
