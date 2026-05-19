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
            setStr(_state.wifiConfiguredSSID, NVSStorage::instance().getWiFiSSID());
            DEBUG_PRINTF("[WiFi] Stored SSID: %s\n", _state.wifiConfiguredSSID);
        }

        // Register WiFi events
        WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
            handleWiFiEvent(event, info);
        });

        DEBUG_PRINTLN("[WiFi] Plugin initialized (STA mode)");
        return true;
    }

    void loop() override {
        // Handle connection timeout
        if (_connecting && millis() - _connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
            _connecting = false;
            _state.wifiConnected = false;
            setStr(_state.otaError, "WiFi connection timeout");
            DEBUG_PRINTLN("[WiFi] Connection timeout");
            sendWiFiStatus();
        }
    }

    // ── Public API ───────────────────────────────────────────

    void configure(const String& ssid, const String& password) {
        NVSStorage::instance().saveWiFiCredentials(ssid, password);
        setStr(_state.wifiConfiguredSSID, ssid);
        DEBUG_PRINTF("[WiFi] Credentials saved for: %s\n", ssid.c_str());
    }

    void connect() {
        if (_state.wifiConnected) {
            DEBUG_PRINTLN("[WiFi] Already connected");
            sendWiFiStatus();
            return;
        }

        if (!NVSStorage::instance().hasWiFiCredentials()) {
            setStr(_state.otaError, "No WiFi credentials configured");
            DEBUG_PRINTLN("[WiFi] No credentials stored");
            sendWiFiStatus();
            return;
        }

        String ssid = NVSStorage::instance().getWiFiSSID();
        String password = NVSStorage::instance().getWiFiPassword();

        DEBUG_PRINTF("[WiFi] Connecting to: %s\n", ssid.c_str());
        _connecting = true;
        _connectStartTime = millis();

        WiFi.begin(ssid.c_str(), password.c_str());
        sendWiFiStatus("connecting");
    }

    void disconnect() {
        DEBUG_PRINTLN("[WiFi] Disconnecting");
        _connecting = false;
        WiFi.disconnect(true);
        _state.wifiConnected = false;
        _state.wifiSSID[0] = 0;
        _state.wifiIP[0]   = 0;
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
        _doc.clear();
        _doc[Protocol::FIELD_TYPE] = Protocol::EVT_WIFI_STATUS;
        _doc["conn"] = _state.wifiConnected;
        _doc["ssid"] = (const char*)_state.wifiSSID;
        _doc["ip"]   = (const char*)_state.wifiIP;
        _doc["cfg"]  = (const char*)_state.wifiConfiguredSSID;  // Configured SSID
        if (status) {
            _doc["st"] = status;
        }
        if (!isEmptyStr(_state.otaError)) {
            _doc["err"] = (const char*)_state.otaError;
        }

        _jsonBuf = "";
        serializeJson(_doc, _jsonBuf);
        bus().publish(Event(EventType::BLESend, _jsonBuf));
    }

private:
    MachineState& _state;
    bool _connecting = false;
    unsigned long _connectStartTime = 0;
    JsonDocument _doc;        // Reused by sendWiFiStatus().
    String       _jsonBuf;    // Reused serialize buffer.

    void handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
        switch (event) {
            case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                _connecting = false;
                _state.wifiConnected = true;
                setStr(_state.wifiSSID, WiFi.SSID());
                setStr(_state.wifiIP,   WiFi.localIP().toString());
                _state.otaError[0] = 0;
                DEBUG_PRINTF("[WiFi] Connected! IP: %s\n", _state.wifiIP);
                sendWiFiStatus();
                bus().publish(EventType::WiFiConnected);
                break;

            case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                if (_state.wifiConnected) {
                    DEBUG_PRINTLN("[WiFi] Disconnected");
                    _state.wifiConnected = false;
                    _state.wifiSSID[0] = 0;
                    _state.wifiIP[0]   = 0;
                    sendWiFiStatus();
                    bus().publish(EventType::WiFiDisconnected);
                }
                break;

            case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                DEBUG_PRINTLN("[WiFi] Associated with AP");
                break;

            default:
                break;
        }
    }
};
