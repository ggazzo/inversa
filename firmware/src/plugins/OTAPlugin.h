#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/OtaVerify.h"
#include "../core/Semver.h"
#include "../core/constants.h"
#include "../models/MachineState.h"
#include "../protocol/protocol.h"
#include "WiFiPlugin.h"

// ─── OTA Plugin ─────────────────────────────────────────────
// Checks GitHub releases for firmware updates and installs them.
// Requires WiFiPlugin to be connected. Verifies ECDSA P-256 signature
// of the binary against the public key in `core/ota_pubkey.h` (P8).
// Rollback (P9) is handled in main.cpp via esp_ota_mark_app_valid_*.

class OTAPlugin : public Plugin {
public:
    // P10 — abort install if no bytes received for this long.
    static constexpr uint32_t STALL_TIMEOUT_MS = 60UL * 1000;
    // P6 — defer ESP.restart() so the final BLE status notify lands.
    static constexpr uint32_t RESTART_GRACE_MS = 1000;
    // Signature size cap (ECDSA P-256 DER is ≤72 bytes).
    static constexpr size_t   MAX_SIG_SIZE     = 128;

    OTAPlugin(MachineState& state, WiFiPlugin& wifi)
        : _state(state), _wifi(wifi) {}

    const char* getName() const override { return "OTA"; }

    bool setup() override {
        DEBUG_PRINTLN("[OTA] Plugin initialized");
        DEBUG_PRINTF("[OTA] Current version: %s\n", BUILD_GIT_VERSION);
        DEBUG_PRINTF("[OTA] Firmware name: %s\n", FIRMWARE_NAME);

        // P7 — dev builds disable OTA silently because Semver::isValidCore("dev")
        // is false. Warn the developer instead.
        if (strcmp(BUILD_GIT_VERSION, "dev") == 0) {
            DEBUG_PRINTLN("[OTA] WARNING: BUILD_GIT_VERSION=\"dev\" — OTA self-disables. Tag a release to enable.");
        }
        return true;
    }

    void loop() override {
        // P6 — graceful deferred restart so the BLE notify has time to flush.
        if (_restartAt && millis() >= _restartAt) {
            _restartAt = 0;
            DEBUG_PRINTLN("[OTA] Restarting now");
            ESP.restart();
        }
    }

    // ── Public API ───────────────────────────────────────────

    void checkForUpdate() {
        if (!_wifi.isConnected()) { setError("WiFi not connected"); return; }

        setStatus("checking");
        DEBUG_PRINTLN("[OTA] Checking for updates...");

        WiFiClientSecure client;
        client.setInsecure();  // MITM mitigated by ECDSA signature check at install time (P8).

        HTTPClient http;
        String url = String(GITHUB_API_URL) + "/repos/" +
                     GITHUB_REPO_OWNER + "/" + GITHUB_REPO_NAME + "/releases/latest";

        http.begin(client, url);
        http.addHeader("User-Agent", "ESP32-BrewPilot");
        http.setTimeout(OTA_CHECK_TIMEOUT_MS);

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK) {
            http.end();
            setError("GitHub API error: " + String(httpCode));
            return;
        }

        String payload = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload);
        if (err) { setError("JSON parse error"); return; }

        const char* tagName = doc["tag_name"] | "";
        if (strlen(tagName) == 0) { setError("No version tag in release"); return; }

        setStr(_state.otaLatestVersion, tagName);
        DEBUG_PRINTF("[OTA] Latest version: %s, Current: %s\n", tagName, BUILD_GIT_VERSION);

        if (!Semver::isNewer(tagName, BUILD_GIT_VERSION)) {
            setStatus("up-to-date");
            DEBUG_PRINTLN("[OTA] Already on latest version");
            return;
        }

        // Find both `<name>.bin` and `<name>.bin.sig` assets — refuse without sig.
        String binAsset = String(FIRMWARE_NAME) + ".bin";
        String sigAsset = binAsset + ".sig";
        String binUrl = "", sigUrl = "";
        size_t binSize = 0;

        JsonArray assets = doc["assets"];
        for (JsonObject asset : assets) {
            String name = asset["name"] | "";
            if (name == binAsset) {
                binUrl  = asset["browser_download_url"] | "";
                binSize = asset["size"] | 0;
            } else if (name == sigAsset) {
                sigUrl  = asset["browser_download_url"] | "";
            }
        }

        if (binUrl.length() == 0) { setError("No firmware for " + binAsset); return; }
        if (sigUrl.length() == 0) {
            // P8 — refuse unsigned releases. Better to brick OTA than accept
            // an unverifiable image.
            setError("Release missing signature " + sigAsset);
            return;
        }

        _downloadUrl  = binUrl;
        _downloadSize = binSize;
        _signatureUrl = sigUrl;

        setStatus("available");
        DEBUG_PRINTF("[OTA] Update available: %s (%u bytes, signed)\n", tagName, (unsigned)binSize);
    }

    void installUpdate() {
        if (!_wifi.isConnected())          { setError("WiFi not connected"); return; }
        if (_downloadUrl.length() == 0)    { setError("No update available"); return; }
        if (_signatureUrl.length() == 0)   { setError("No signature available"); return; }

        // P8 — download signature first (small, fail-fast).
        uint8_t  sig[MAX_SIG_SIZE];
        size_t   sigLen = 0;
        if (!downloadSignature(sig, sizeof(sig), sigLen)) return;  // already setError'd

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
        if (httpCode != HTTP_CODE_OK) { http.end(); setError("Download error: " + String(httpCode)); return; }

        int contentLength = http.getSize();
        if (contentLength <= 0) { http.end(); setError("Invalid content length"); return; }

        DEBUG_PRINTF("[OTA] Content length: %d bytes\n", contentLength);

        if (!Update.begin(contentLength)) { http.end(); setError("Not enough space for update"); return; }

        OtaVerify verifier;
        if (!verifier.begin()) { http.end(); Update.abort(); setError("Hash init failed"); return; }

        WiFiClient* stream = http.getStreamPtr();
        uint8_t  buffer[1024];
        size_t   written = 0;
        int      lastReportedPct = -1;
        uint32_t lastProgressMs = millis();

        while (http.connected() && written < (size_t)contentLength) {
            // P10 — stall detection.
            if (millis() - lastProgressMs > STALL_TIMEOUT_MS) {
                http.end(); Update.abort();
                setError("Download stalled (no progress for 60s)");
                return;
            }

            size_t available = stream->available();
            if (available > 0) {
                size_t toRead = min(available, sizeof(buffer));
                size_t read = stream->readBytes(buffer, toRead);

                if (Update.write(buffer, read) != read) {
                    http.end(); Update.abort();
                    setError("Write error during update");
                    return;
                }
                verifier.update(buffer, read);

                written += read;
                lastProgressMs = millis();

                int pct = (written * 100) / contentLength;
                if (pct != lastReportedPct && (pct % OTA_PROGRESS_INTERVAL_PCT == 0 || pct == 100)) {
                    lastReportedPct = pct;
                    _state.otaProgress = pct;
                    sendStatus();
                    DEBUG_PRINTF("[OTA] Progress: %d%%\n", pct);
                }
            }
            delay(1);  // yield to watchdog
        }

        http.end();

        if (written != (size_t)contentLength) {
            Update.abort();
            setError("Download incomplete");
            return;
        }

        // P8 — verify before committing the image.
        setStatus("verifying");
        if (!verifier.finishAndVerify(sig, sigLen)) {
            Update.abort();
            setError("Signature verification FAILED");
            return;
        }

        if (!Update.end(true)) {
            setError("Update finalization failed");
            return;
        }

        setStatus("installing");
        DEBUG_PRINTLN("[OTA] Update complete, restarting...");

        // P6 — defer restart so the BLE notify above lands at the app.
        _restartAt = millis() + RESTART_GRACE_MS;
    }

private:
    MachineState& _state;
    WiFiPlugin&   _wifi;
    String        _downloadUrl  = "";
    size_t        _downloadSize = 0;
    String        _signatureUrl = "";
    uint32_t      _restartAt    = 0;
    JsonDocument  _doc;       // Reused by sendStatus() — avoids per-status heap churn.
    String        _jsonBuf;   // Reused serialize buffer (retains capacity across calls).

    // P8 — download `<bin>.sig` into `out` (ECDSA DER, ≤72 bytes).
    bool downloadSignature(uint8_t* out, size_t maxLen, size_t& outLen) {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setRedirectLimit(5);
        http.begin(client, _signatureUrl);
        http.setTimeout(OTA_CHECK_TIMEOUT_MS);

        int code = http.GET();
        if (code != HTTP_CODE_OK) { http.end(); setError("Signature fetch failed: " + String(code)); return false; }

        int len = http.getSize();
        if (len <= 0 || (size_t)len > maxLen) {
            http.end();
            setError("Signature size invalid");
            return false;
        }
        WiFiClient* s = http.getStreamPtr();
        size_t got = s->readBytes(out, len);
        http.end();
        if (got != (size_t)len) { setError("Signature read short"); return false; }
        outLen = got;
        return true;
    }

    void setStatus(const char* status) {
        setStr(_state.otaStatus, status);
        _state.otaError[0] = 0;
        sendStatus();
    }

    void setError(const String& error) {
        setStr(_state.otaStatus, "error");
        setStr(_state.otaError, error);
        DEBUG_PRINTF("[OTA] Error: %s\n", error.c_str());
        sendStatus();
    }

    void sendStatus() {
        _doc.clear();
        _doc[Protocol::FIELD_TYPE] = Protocol::EVT_OTA_STATUS;
        _doc["st"]  = (const char*)_state.otaStatus;
        _doc["ver"] = (const char*)_state.otaLatestVersion;
        _doc["pct"] = _state.otaProgress;
        _doc["cur"] = BUILD_GIT_VERSION;
        if (!isEmptyStr(_state.otaError)) _doc["err"] = (const char*)_state.otaError;

        _jsonBuf = "";
        serializeJson(_doc, _jsonBuf);
        bus().publish(Event(EventType::BLESend, _jsonBuf));
    }
};
