#pragma once

// Standalone mDNS responder. Advertises the device on the local
// network as `<hostname>.local` so anything on the LAN (Home Assistant
// discovery, scripts, browsers) can resolve the brewer without
// hard-coding its IP. Hostname comes from NVS (`dev_name`, set by the
// `req:device:rename` BLE command); if unset, falls back to
// `brewpilot-<chip-mac-suffix>` so multiple devices on the same LAN
// don't collide.
//
// Runs in every build — production and dev-OTA alike. ArduinoOTAPlugin
// later piggybacks on this MDNS instance by adding its own
// `_arduino._tcp` service via `ArduinoOTA.begin()` (which is a no-op
// against `MDNS.begin` since it just enables a service entry).

#include <Arduino.h>
#include <ESPmDNS.h>
#include "../core/Plugin.h"
#include "../core/NVSStorage.h"

class MDNSPlugin : public Plugin {
public:
    const char* getName() const override { return "MDNS"; }

    bool setup() override {
        bus().subscribe(EventType::WiFiConnected, [this](const Event&) {
            startResponder();
        });
        bus().subscribe(EventType::WiFiDisconnected, [this](const Event&) {
            if (_running) {
                MDNS.end();
                _running = false;
                DEBUG_PRINTLN("[mDNS] Stopped (WiFi disconnected)");
            }
        });
        return true;
    }

    void loop() override {}

    const String& hostname() const { return _hostname; }

private:
    bool   _running = false;
    String _hostname;

    void startResponder() {
        if (_running) return;
        _hostname = resolveHostname();
        if (MDNS.begin(_hostname.c_str())) {
            // Advertise a discoverable service so HA / GaggiMate-style
            // integrations can find the brewer by service type instead
            // of a hard-coded hostname.
            MDNS.addService("brewpilot", "tcp", 80);
            DEBUG_PRINTF("[mDNS] Advertising as %s.local\n", _hostname.c_str());
            _running = true;
        } else {
            DEBUG_PRINTF("[mDNS] begin(%s) failed\n", _hostname.c_str());
        }
    }

    // Hostname is sanitized: lowercase, alphanumeric + dash only, max 63
    // chars (RFC 1035 label limit). Falls back to a stable per-device
    // hostname when the user hasn't picked one.
    static String resolveHostname() {
        String name = NVSStorage::instance().loadDeviceName(String());
        if (name.length() == 0) {
            uint64_t mac = ESP.getEfuseMac();
            char fallback[32];
            snprintf(fallback, sizeof(fallback), "brewpilot-%04x",
                     (uint16_t)((mac >> 32) & 0xFFFF));
            return String(fallback);
        }
        String sanitized;
        sanitized.reserve(name.length());
        for (size_t i = 0; i < name.length() && sanitized.length() < 63; ++i) {
            char c = name[i];
            if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
                sanitized += c;
            } else if (c == ' ' || c == '_') {
                sanitized += '-';
            }
            // drop anything else
        }
        // mDNS label cannot start or end with a dash
        while (sanitized.startsWith("-")) sanitized = sanitized.substring(1);
        while (sanitized.endsWith("-"))   sanitized = sanitized.substring(0, sanitized.length() - 1);
        if (sanitized.length() == 0) {
            uint64_t mac = ESP.getEfuseMac();
            char fallback[32];
            snprintf(fallback, sizeof(fallback), "brewpilot-%04x",
                     (uint16_t)((mac >> 32) & 0xFFFF));
            return String(fallback);
        }
        return sanitized;
    }
};
