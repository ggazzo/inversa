#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/constants.h"
#include "../core/Semver.h"
#include "../models/MachineState.h"
#include "../protocol/protocol.h"
#include "WiFiPlugin.h"

// ─── OTA Plugin ─────────────────────────────────────────────
// Checks GitHub releases for firmware updates and installs them.
// Requires WiFiPlugin to be connected.

class OTAPlugin : public Plugin {
public:
    OTAPlugin(MachineState& state, WiFiPlugin& wifi) 
        : _state(state), _wifi(wifi) {}

    const char* getName() const override { return "OTA"; }

    bool setup() override {
        DEBUG_PRINTLN("[OTA] Plugin initialized");
        DEBUG_PRINTF("[OTA] Current version: %s\n", BUILD_GIT_VERSION);
        DEBUG_PRINTF("[OTA] Firmware name: %s\n", FIRMWARE_NAME);
        return true;
    }

    void loop() override {
        // Nothing to do in loop - all actions are triggered by commands
    }

    // ── Public API ───────────────────────────────────────────

    void checkForUpdate() {
        if (!_wifi.isConnected()) {
            setError("WiFi not connected");
            return;
        }

        setStatus("checking");
        DEBUG_PRINTLN("[OTA] Checking for updates...");

        WiFiClientSecure client;
        client.setInsecure();  // Skip certificate validation

        HTTPClient http;
        String url = String(GITHUB_API_URL) + "/repos/" + 
                     GITHUB_REPO_OWNER + "/" + GITHUB_REPO_NAME + "/releases/latest";

        http.begin(client, url);
        http.addHeader("User-Agent", "ESP32-Inversa");
        http.setTimeout(OTA_CHECK_TIMEOUT_MS);

        int httpCode = http.GET();
        
        if (httpCode != HTTP_CODE_OK) {
            http.end();
            setError("GitHub API error: " + String(httpCode));
            return;
        }

        String payload = http.getString();
        http.end();

        // Parse JSON response
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (err) {
            setError("JSON parse error");
            return;
        }

        const char* tagName = doc["tag_name"] | "";
        if (strlen(tagName) == 0) {
            setError("No version tag in release");
            return;
        }

        _state.otaLatestVersion = tagName;
        DEBUG_PRINTF("[OTA] Latest version: %s, Current: %s\n", 
                      tagName, BUILD_GIT_VERSION);

        // Compare versions
        if (!Semver::isNewer(tagName, BUILD_GIT_VERSION)) {
            setStatus("up-to-date");
            DEBUG_PRINTLN("[OTA] Already on latest version");
            return;
        }

        // Find firmware asset matching our board
        String assetName = String(FIRMWARE_NAME) + ".bin";
        String downloadUrl = "";
        size_t assetSize = 0;

        JsonArray assets = doc["assets"];
        for (JsonObject asset : assets) {
            String name = asset["name"] | "";
            if (name == assetName) {
                downloadUrl = asset["browser_download_url"] | "";
                assetSize = asset["size"] | 0;
                break;
            }
        }

        if (downloadUrl.length() == 0) {
            setError("No firmware for " + assetName);
            return;
        }

        _downloadUrl = downloadUrl;
        _downloadSize = assetSize;
        
        setStatus("available");
        DEBUG_PRINTF("[OTA] Update available: %s (%d bytes)\n", 
                      tagName, assetSize);
    }

    void installUpdate() {
        if (!_wifi.isConnected()) {
            setError("WiFi not connected");
            return;
        }

        if (_downloadUrl.length() == 0) {
            setError("No update available");
            return;
        }

        setStatus("downloading");
        _state.otaProgress = 0;
        sendStatus();

        DEBUG_PRINTF("[OTA] Downloading: %s\n", _downloadUrl.c_str());

        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setRedirectLimit(5);
        http.begin(client, _downloadUrl);
        http.setTimeout(OTA_CHECK_TIMEOUT_MS);

        int httpCode = http.GET();
        
        if (httpCode != HTTP_CODE_OK) {
            http.end();
            setError("Download error: " + String(httpCode));
            return;
        }

        int contentLength = http.getSize();
        if (contentLength <= 0) {
            http.end();
            setError("Invalid content length");
            return;
        }

        DEBUG_PRINTF("[OTA] Content length: %d bytes\n", contentLength);

        if (!Update.begin(contentLength)) {
            http.end();
            setError("Not enough space for update");
            return;
        }

        WiFiClient* stream = http.getStreamPtr();
        
        // Download with progress reporting
        uint8_t buffer[1024];
        size_t written = 0;
        int lastReportedPct = -1;

        while (http.connected() && written < (size_t)contentLength) {
            size_t available = stream->available();
            if (available > 0) {
                size_t toRead = min(available, sizeof(buffer));
                size_t read = stream->readBytes(buffer, toRead);
                
                if (Update.write(buffer, read) != read) {
                    http.end();
                    Update.abort();
                    setError("Write error during update");
                    return;
                }
                
                written += read;
                
                // Report progress
                int pct = (written * 100) / contentLength;
                if (pct != lastReportedPct && (pct % OTA_PROGRESS_INTERVAL_PCT == 0 || pct == 100)) {
                    lastReportedPct = pct;
                    _state.otaProgress = pct;
                    sendStatus();
                    DEBUG_PRINTF("[OTA] Progress: %d%%\n", pct);
                }
            }
            delay(1);  // Yield to watchdog
        }

        http.end();

        if (written != (size_t)contentLength) {
            Update.abort();
            setError("Download incomplete");
            return;
        }

        if (!Update.end(true)) {
            setError("Update finalization failed");
            return;
        }

        setStatus("installing");
        DEBUG_PRINTLN("[OTA] Update complete, restarting...");
        
        delay(1000);  // Give time for BLE message to send
        ESP.restart();
    }

private:
    MachineState& _state;
    WiFiPlugin& _wifi;
    String _downloadUrl = "";
    size_t _downloadSize = 0;

    void setStatus(const char* status) {
        _state.otaStatus = status;
        _state.otaError = "";
        sendStatus();
    }

    void setError(const String& error) {
        _state.otaStatus = "error";
        _state.otaError = error;
        DEBUG_PRINTF("[OTA] Error: %s\n", error.c_str());
        sendStatus();
    }

    void sendStatus() {
        JsonDocument doc;
        doc[Protocol::FIELD_TYPE] = Protocol::EVT_OTA_STATUS;
        doc["st"] = _state.otaStatus;
        doc["ver"] = _state.otaLatestVersion;
        doc["pct"] = _state.otaProgress;
        doc["cur"] = BUILD_GIT_VERSION;
        if (_state.otaError.length() > 0) {
            doc["err"] = _state.otaError;
        }

        String json;
        serializeJson(doc, json);
        bus().publish(Event(EventType::BLESend, json));
    }
};
