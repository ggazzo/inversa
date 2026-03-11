#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
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
        NimBLEDevice::setMTU(512);

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

        Serial.println("[BLE] Service started, advertising...");
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
        if (!_connected || !_txChar) return;

        // BLE MTU chunking for large messages
        const uint8_t* data = (const uint8_t*)json.c_str();
        size_t len = json.length();
        size_t mtu = NimBLEDevice::getMTU() - 3;  // 3 bytes overhead

        for (size_t i = 0; i < len; i += mtu) {
            size_t chunk = min(mtu, len - i);
            _txChar->setValue(data + i, chunk);
            _txChar->notify();
            if (len > mtu) delay(20);  // small delay between chunks
        }
    }

    void sendJson(JsonDocument& doc) {
        String json;
        serializeJson(doc, json);
        send(json);
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
    bool _connected = false;
    uint32_t _lastTelemetry = 0;
    String _rxBuffer = "";

    // ── NimBLE Server Callbacks ─────────────────────────────

    void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
        _connected = true;
        gState.bleConnected = true;
        bus().publish(EventType::BLEClientConnected);
        Serial.printf("[BLE] Client connected: %s\n", 
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
        doc["recipe"] = gState.recoveryRecipeName;
        sendJson(doc);
        Serial.printf("[BLE] Sent recovery notification: %s\n", 
                     gState.recoveryRecipeName.c_str());
    }

    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
        _connected = (server->getConnectedCount() > 0);
        gState.bleConnected = _connected;
        if (!_connected) {
            bus().publish(EventType::BLEClientDisconnected);
        }
        Serial.printf("[BLE] Client disconnected (reason=%d)\n", reason);
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
            Serial.printf("[BLE] JSON parse error: %s\n", err.c_str());
            _rxBuffer = "";
        }
        // If IncompleteInput, keep accumulating
    }

    // ── Command Handler ─────────────────────────────────────

    void handleCommand(JsonDocument& doc) {
        const char* type = doc[Protocol::FIELD_TYPE] | "";
        String rid = doc[Protocol::FIELD_REQUEST_ID] | "";

        Serial.printf("[BLE] Command: %s\n", type);

        // Publish raw command for other plugins
        String raw;
        serializeJson(doc, raw);
        bus().publish(EventType::BLECommandReceived, raw);
    }

    // ── Telemetry ───────────────────────────────────────────

    void sendTelemetry() {
        JsonDocument doc;
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

        // Recipe info (when running)
        if (gState.mode == OperatingMode::Recipe) {
            doc[Protocol::FIELD_RECIPE_STEP]  = gState.recipeStep;
            doc[Protocol::FIELD_RECIPE_TOTAL] = gState.recipeTotalSteps;
            doc[Protocol::FIELD_RECIPE_NAME]  = gState.recipeName;
            doc[Protocol::FIELD_TIMER_LEFT]   = gState.timerRemainingMs / 1000;
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

        sendJson(doc);
    }

    float round2(float val) {
        return round(val * 100.0f) / 100.0f;
    }
};
