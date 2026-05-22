#pragma once

// Dev-only OTA via ArduinoOTA / espota. Lets PlatformIO push builds over WiFi
// during development without USB:
//
//   pio run -e wemos_s3_mini_devota -t upload --upload-port inversa.local
//
// Gated by -DDEV_OTA_ENABLED. NEVER ship this in release builds — it ignores
// the ECDSA signature path that OTAPlugin enforces. A password is mandatory:
// without -DDEV_OTA_PASSWORD the build is refused.

#ifdef DEV_OTA_ENABLED

#ifndef DEV_OTA_PASSWORD
#error "DEV_OTA_ENABLED requires -DDEV_OTA_PASSWORD=\\\"...\\\" build flag"
#endif

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include "../core/Plugin.h"
#include "../core/constants.h"

class ArduinoOTAPlugin : public Plugin {
public:
    const char* getName() const override { return "ArduinoOTA"; }

    bool setup() override {
        // Defer ArduinoOTA.begin() until WiFi is up — it needs an IP to bind
        // the listening socket and to register the mDNS record.
        bus().subscribe(EventType::WiFiConnected, [this](const Event&) {
            startServer();
        });
        bus().subscribe(EventType::WiFiDisconnected, [this](const Event&) {
            _running = false;
        });

        DEBUG_PRINTLN("[ArduinoOTA] Armed — waiting for WiFi");
        return true;
    }

    void loop() override {
        if (_running) ArduinoOTA.handle();
    }

private:
    bool _running = false;

    void startServer() {
        if (_running) return;

        // Hostname uses the chip MAC suffix so multiple devices on the same
        // LAN don't collide on `inversa.local`.
        uint64_t mac = ESP.getEfuseMac();
        char host[32];
        snprintf(host, sizeof(host), "inversa-%04x",
                 (uint16_t)((mac >> 32) & 0xFFFF));
        ArduinoOTA.setHostname(host);
        ArduinoOTA.setPassword(DEV_OTA_PASSWORD);

        ArduinoOTA.onStart([]() {
            DEBUG_PRINTLN("[ArduinoOTA] Update starting");
        });
        ArduinoOTA.onEnd([]() {
            DEBUG_PRINTLN("[ArduinoOTA] Update done — rebooting");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            static uint8_t lastPct = 255;
            uint8_t pct = (uint8_t)((progress * 100UL) / total);
            if (pct != lastPct) {
                DEBUG_PRINTF("[ArduinoOTA] %u%%\n", pct);
                lastPct = pct;
            }
        });
        ArduinoOTA.onError([](ota_error_t err) {
            DEBUG_PRINTF("[ArduinoOTA] Error %u\n", err);
        });

        ArduinoOTA.begin();
        _running = true;
        DEBUG_PRINTF("[ArduinoOTA] Listening as %s.local\n", host);
    }
};

#endif  // DEV_OTA_ENABLED
