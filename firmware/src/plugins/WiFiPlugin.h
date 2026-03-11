#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/NVSStorage.h"
#include "../core/constants.h"
#include "../models/MachineState.h"
#include "../protocol/protocol.h"

// ─── WiFi Plugin ────────────────────────────────────────────
// Manages WiFi connection for OTA updates.
// Credentials are configured via BLE and stored in NVS.

class WiFiPlugin : public Plugin {
public:
    WiFiPlugin(MachineState& state) : _state(state) {}

    const char* getName() const override { return "WiFi"; }

    bool setup() override {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);

        // Load stored SSID for display (don't auto-connect)
        if (NVSStorage::instance().hasWiFiCredentials()) {
            _state.wifiConfiguredSSID = NVSStorage::instance().getWiFiSSID();
            Serial.printf("[WiFi] Stored SSID: %s\n", _state.wifiConfiguredSSID.c_str());
        }

        // Register WiFi events
        WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
            handleWiFiEvent(event, info);
        });

        Serial.println("[WiFi] Plugin initialized (STA mode)");
        return true;
    }

    void loop() override {
        // Handle connection timeout
        if (_connecting && millis() - _connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
            _connecting = false;
            _state.wifiConnected = false;
            _state.otaError = "WiFi connection timeout";
            Serial.println("[WiFi] Connection timeout");
            sendWiFiStatus();
        }
    }

    // ── Public API ───────────────────────────────────────────

    void configure(const String& ssid, const String& password) {
        NVSStorage::instance().saveWiFiCredentials(ssid, password);
        _state.wifiConfiguredSSID = ssid;
        Serial.printf("[WiFi] Credentials saved for: %s\n", ssid.c_str());
    }

    void connect() {
        if (_state.wifiConnected) {
            Serial.println("[WiFi] Already connected");
            sendWiFiStatus();
            return;
        }

        if (!NVSStorage::instance().hasWiFiCredentials()) {
            _state.otaError = "No WiFi credentials configured";
            Serial.println("[WiFi] No credentials stored");
            sendWiFiStatus();
            return;
        }

        String ssid = NVSStorage::instance().getWiFiSSID();
        String password = NVSStorage::instance().getWiFiPassword();

        Serial.printf("[WiFi] Connecting to: %s\n", ssid.c_str());
        _connecting = true;
        _connectStartTime = millis();

        WiFi.begin(ssid.c_str(), password.c_str());
        sendWiFiStatus("connecting");
    }

    void disconnect() {
        Serial.println("[WiFi] Disconnecting");
        _connecting = false;
        WiFi.disconnect(true);
        _state.wifiConnected = false;
        _state.wifiSSID = "";
        _state.wifiIP = "";
        sendWiFiStatus();
    }

    bool isConnected() const {
        return WiFi.status() == WL_CONNECTED;
    }

    String getIP() const {
        return WiFi.localIP().toString();
    }

    String getSSID() const {
        return WiFi.SSID();
    }

    void sendWiFiStatus(const char* status = nullptr) {
        JsonDocument doc;
        doc[Protocol::FIELD_TYPE] = Protocol::EVT_WIFI_STATUS;
        doc["conn"] = _state.wifiConnected;
        doc["ssid"] = _state.wifiSSID;
        doc["ip"] = _state.wifiIP;
        doc["cfg"] = _state.wifiConfiguredSSID;  // Configured SSID
        if (status) {
            doc["st"] = status;
        }
        if (_state.otaError.length() > 0) {
            doc["err"] = _state.otaError;
        }

        String json;
        serializeJson(doc, json);
        bus().publish(Event{EventType::BLESend, json});
    }

private:
    MachineState& _state;
    bool _connecting = false;
    unsigned long _connectStartTime = 0;

    void handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                _connecting = false;
                _state.wifiConnected = true;
                _state.wifiSSID = WiFi.SSID();
                _state.wifiIP = WiFi.localIP().toString();
                _state.otaError = "";
                Serial.printf("[WiFi] Connected! IP: %s\n", _state.wifiIP.c_str());
                sendWiFiStatus();
                break;

            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                if (_state.wifiConnected) {
                    Serial.println("[WiFi] Disconnected");
                    _state.wifiConnected = false;
                    _state.wifiSSID = "";
                    _state.wifiIP = "";
                    sendWiFiStatus();
                }
                break;

            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                Serial.println("[WiFi] Associated with AP");
                break;

            default:
                break;
        }
    }
};
