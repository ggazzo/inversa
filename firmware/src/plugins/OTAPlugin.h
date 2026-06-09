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
#include "../core/NVSStorage.h"
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

    // Auto-update: give up on a target after this many failed attempts
    // (download/verify fail, or boot-then-rollback) so a bad build can't loop.
    static constexpr uint8_t AUTO_MAX_TRIES = 3;
    // Settle delay after WiFi connects before the one-shot auto-check.
    static constexpr uint32_t AUTO_CHECK_DELAY_MS = 3000;

    bool setup() override {
        DEBUG_PRINTLN("[OTA] Plugin initialized");
        DEBUG_PRINTF("[OTA] Current version: %s\n", BUILD_GIT_VERSION);
        DEBUG_PRINTF("[OTA] Firmware name: %s\n", FIRMWARE_NAME);

        // P7 — dev builds disable OTA silently because Semver::isValidCore("dev")
        // is false. Warn the developer instead.
        if (strcmp(BUILD_GIT_VERSION, "dev") == 0) {
            DEBUG_PRINTLN("[OTA] WARNING: BUILD_GIT_VERSION=\"dev\" — OTA self-disables. Tag a release to enable.");
        }

        // Arm a one-shot auto-check shortly after the first WiFi connect. Run
        // from loop() (not inline in the event) so it has a full stack and
        // doesn't block the EventBus during the blocking HTTPS calls.
        bus().subscribe(EventType::WiFiConnected, [this](const Event&) {
            if (!_autoChecked && _autoPendingAt == 0) {
                _autoPendingAt = millis() + AUTO_CHECK_DELAY_MS;
            }
        });
        return true;
    }

    void loop() override {
        // P6 — graceful deferred restart so the BLE notify has time to flush.
        if (_restartAt && millis() >= _restartAt) {
            _restartAt = 0;
            DEBUG_PRINTLN("[OTA] Restarting now");
            ESP.restart();
        }

        if (_autoPendingAt && millis() >= _autoPendingAt) {
            _autoPendingAt = 0;
            autoCheck();
        }
    }

    // ── Public API ───────────────────────────────────────────

    // Manual check against the production channel (releases/latest), forward-
    // only via semver. Sets up an install the user then confirms.
    void checkForUpdate() {
        setStatus("checking");
        ReleaseInfo r = resolveRelease(String("production"));
        if (!r.ok) return;  // resolveRelease already setError'd

        setStr(_state.otaLatestVersion, r.tag.c_str());
        if (!Semver::isNewer(r.tag.c_str(), BUILD_GIT_VERSION)) {
            setStatus("up-to-date");
            DEBUG_PRINTLN("[OTA] Already on latest version");
            return;
        }
        _downloadUrl  = r.binUrl;
        _downloadSize = r.binSize;
        _signatureUrl = r.sigUrl;
        setStatus("available");
        DEBUG_PRINTF("[OTA] Update available: %s (%u bytes, signed)\n",
                     r.tag.c_str(), (unsigned)r.binSize);
    }

    // ── Boot-time auto-update (by channel/tag, loop-protected) ───────────
    // Runs once per boot, ~3s after WiFi connects. Anti-loop layers:
    //   1. Once per boot (not a poll); the bootloader handles crash-loops via
    //      rollback (P9) independently.
    //   2. Skip while this very boot is still PENDING_VERIFY — don't stack a
    //      second update before the current one proves stable.
    //   3. Skip if the resolved build's install-identity (tag@asset_updated_at)
    //      equals the last CONFIRMED one — never reinstall what's running.
    //   4. Poison a target after AUTO_MAX_TRIES failed attempts (the attempt is
    //      persisted BEFORE reboot, so a brick/rollback counts too).
    //   5. production also keeps the forward-only semver gate.
    void autoCheck() {
        if (_autoChecked) return;
        _autoChecked = true;

        // (2) current image not yet confirmed → let it settle first.
        if (_state.otaVerifyDeadline > 0) {
            DEBUG_PRINTLN("[OTA] auto: image pending verify — deferring");
            return;
        }

        auto& nvs = NVSStorage::instance();
        String ch = nvs.loadOtaChannel("production");
        bool isProd = (ch.length() == 0 || ch == "production");
        // Default ON for non-production channels (dev/rc/pinned tag); OFF for
        // production unless the user opted in.
        bool autoOn = nvs.hasOtaAuto() ? nvs.loadOtaAuto(false) : !isProd;
        if (!autoOn) {
            DEBUG_PRINTF("[OTA] auto: disabled (channel=%s)\n", ch.c_str());
            return;
        }

        DEBUG_PRINTF("[OTA] auto: checking channel '%s'\n", ch.c_str());
        ReleaseInfo r = resolveRelease(ch);
        if (!r.ok) return;  // setError'd

        setStr(_state.otaLatestVersion, r.tag.c_str());
        String iid = r.tag + "@" + r.assetUpdatedAt;

        // (3) already on this exact build.
        if (iid == nvs.loadOtaDone(String())) { setStatus("up-to-date"); return; }

        // (5) production: never go backwards.
        if (isProd && !Semver::isNewer(r.tag.c_str(), BUILD_GIT_VERSION)) {
            setStatus("up-to-date");
            return;
        }

        // (4) poison check — count attempts against this specific iid.
        uint8_t tries = (iid == nvs.loadOtaTarget(String())) ? nvs.loadOtaTries(0) : 0;
        if (tries >= AUTO_MAX_TRIES) {
            setError("auto-update halted for " + iid + " after " + String(tries) + " tries");
            return;
        }

        // Persist the attempt BEFORE installing so a brick/rollback still
        // increments the counter and eventually poisons the bad build.
        nvs.saveOtaTarget(iid);
        nvs.saveOtaTries(tries + 1);

        DEBUG_PRINTF("[OTA] auto: installing %s (attempt %u/%u)\n",
                     iid.c_str(), tries + 1, AUTO_MAX_TRIES);
        _downloadUrl  = r.binUrl;
        _downloadSize = r.binSize;
        _signatureUrl = r.sigUrl;
        installUpdate();  // reboots on success; verify is enforced (P8)
    }

    // Install a specific build chosen from the catalog. Unlike checkForUpdate
    // + installUpdate (which only ever targets releases/latest and refuses a
    // non-newer tag), this installs whatever signed build the user picks —
    // older, dev, or a different channel. Safe because installUpdate() still
    // verifies the ECDSA signature against the embedded pubkey (P8): only a
    // build signed with the project key installs, regardless of the URL.
    void installBuild(const String& binUrl, const String& sigUrl, const String& version) {
        if (binUrl.length() == 0 || sigUrl.length() == 0) {
            setError("install-build needs url + sig");
            return;
        }
        _downloadUrl  = binUrl;
        _signatureUrl = sigUrl;
        _downloadSize = 0;  // unknown up front; installUpdate uses Content-Length
        setStr(_state.otaLatestVersion, version.c_str());
        installUpdate();
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
    bool          _autoChecked  = false;  // one-shot boot auto-check guard
    uint32_t      _autoPendingAt = 0;     // scheduled autoCheck() time (0 = none)
    JsonDocument  _doc;       // Reused by sendStatus() — avoids per-status heap churn.
    String        _jsonBuf;   // Reused serialize buffer (retains capacity across calls).

    // A release resolved from a channel: the firmware asset + the bits the
    // auto-updater needs to identify and gate it.
    struct ReleaseInfo {
        String tag;
        String binUrl;
        String sigUrl;
        String assetUpdatedAt;   // GitHub asset `updated_at` — bumps on re-upload
        size_t binSize = 0;
        bool   ok = false;
    };

    // Fetch + parse a release for `channel`: "" / "production" → releases/latest,
    // anything else → releases/tags/<channel> (dev, rc, or a pinned vX.Y.Z).
    // Refuses a release without the matching signed `<FIRMWARE_NAME>.bin[.sig]`.
    ReleaseInfo resolveRelease(const String& channel) {
        ReleaseInfo r;
        if (!_wifi.isConnected()) { setError("WiFi not connected"); return r; }

        WiFiClientSecure client;
        client.setInsecure();  // MITM mitigated by ECDSA signature check (P8).
        HTTPClient http;
        String path = (channel.length() == 0 || channel == "production")
                          ? String("/releases/latest")
                          : ("/releases/tags/" + channel);
        String url = String(GITHUB_API_URL) + "/repos/" +
                     GITHUB_REPO_OWNER + "/" + GITHUB_REPO_NAME + path;
        http.begin(client, url);
        http.addHeader("User-Agent", "ESP32-BrewPilot");
        http.setTimeout(OTA_CHECK_TIMEOUT_MS);

        int code = http.GET();
        if (code != HTTP_CODE_OK) { http.end(); setError("GitHub API error: " + String(code)); return r; }
        String payload = http.getString();
        http.end();

        JsonDocument doc;
        if (deserializeJson(doc, payload)) { setError("JSON parse error"); return r; }

        r.tag = (const char*)(doc["tag_name"] | "");
        if (r.tag.length() == 0) { setError("No version tag in release"); return r; }

        String binAsset = String(FIRMWARE_NAME) + ".bin";
        String sigAsset = binAsset + ".sig";
        for (JsonObject asset : doc["assets"].as<JsonArray>()) {
            String name = asset["name"] | "";
            if (name == binAsset) {
                r.binUrl         = (const char*)(asset["browser_download_url"] | "");
                r.binSize        = asset["size"] | 0;
                r.assetUpdatedAt = (const char*)(asset["updated_at"] | "");
            } else if (name == sigAsset) {
                r.sigUrl = (const char*)(asset["browser_download_url"] | "");
            }
        }
        if (r.binUrl.length() == 0) { setError("No firmware for " + binAsset); return r; }
        if (r.sigUrl.length() == 0) { setError("Release missing signature " + sigAsset); return r; }
        r.ok = true;
        return r;
    }

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
