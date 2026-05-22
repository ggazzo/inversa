// BleClient — minimal NimBLE central skeleton.
//
// Behavior:
//   1. On boot: scan continuously for the brewing controller, matching
//      either the legacy 16-bit advertised name "Inversa" or the device
//      name reported in scan response.
//   2. On match: connect, discover NUS, subscribe to TX notifications.
//   3. Each notify arrives as one JSON line (controller sends one
//      command per notify after the chunked-notify rework). Parse with
//      ArduinoJson and copy the relevant fields into AppState.
//   4. Outbound: assemble a small JsonDocument, serialize, write to RX.
//
// Failure handling kept intentionally simple — disconnect just sends us
// back to scanning. There is no bonding; the device is single-purpose.

#include "BleClient.h"
#include "../state/AppState.h"
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include <cstring>

namespace inversa { namespace display {

namespace {

constexpr const char* NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* NUS_TX_UUID      = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";  // notify
constexpr const char* NUS_RX_UUID      = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";  // write

NimBLEClient*         g_client = nullptr;
NimBLERemoteCharacteristic* g_rx = nullptr;

// Update a char-array field in AppState under a small critical section.
// The state struct keeps atomics for primitives; string fields use this
// helper so partial copies are not observed by the UI tick.
void copyStr(char* dst, size_t cap, const char* src) {
    if (!src) { dst[0] = 0; return; }
    strncpy(dst, src, cap - 1);
    dst[cap - 1] = 0;
}

void handleStatus(const JsonDocument& doc) {
    auto& s = app_state();
    // Keys match firmware/src/protocol/protocol.h FIELD_*. The display's
    // previous tc/tg/po names predated the controller's protocol freeze
    // and were silently parsing nothing → every reading stayed at 0.
    if (doc.containsKey("ct"))  s.currentTemp.store(doc["ct"].as<float>());
    if (doc.containsKey("tt"))  s.targetTemp.store(doc["tt"].as<float>());
    if (doc.containsKey("out")) s.pidOutput.store(doc["out"].as<float>());
    if (doc.containsKey("h"))   s.heaterOn.store(doc["h"].as<bool>());
    if (doc.containsKey("p"))   s.pumpOn.store(doc["p"].as<bool>());
    if (doc.containsKey("m"))   copyStr(s.mode, sizeof(s.mode), doc["m"].as<const char*>());

    // Recipe — controller emits step/total/name + the `rst` sub-state
    // string ("idle", "running", "paused", "waiting_temp", "waiting_timer",
    // "waiting_confirm", "completed"). See BLEPlugin.h::sendTelemetry.
    if (doc.containsKey("rs"))  s.recipeStep.store(doc["rs"].as<int>());
    if (doc.containsKey("rt"))  s.recipeTotal.store(doc["rt"].as<int>());
    if (doc.containsKey("rn"))  copyStr(s.recipeName,  sizeof(s.recipeName),  doc["rn"].as<const char*>());
    if (doc.containsKey("rst")) copyStr(s.recipeState, sizeof(s.recipeState), doc["rst"].as<const char*>());

    // Top-level WAIT_CONFIRM gate (`wc`) + optional confirm message
    // (`cm`). Always emitted by the controller — UI clears the modal
    // when wc flips back to false.
    if (doc.containsKey("wc")) s.waitingForConfirm.store(doc["wc"].as<bool>());
    if (doc.containsKey("cm")) copyStr(s.confirmMessage, sizeof(s.confirmMessage), doc["cm"].as<const char*>());

    // Boil sub-state, only present when active. ba=active, bt=total s,
    // br=remaining s. evt:boil:status carries the same fields plus a
    // status string and is handled separately.
    s.boilActive.store(doc["ba"].as<bool>());  // implicit false if absent
    if (doc.containsKey("bt")) s.boilTotal.store(doc["bt"].as<int>());
    if (doc.containsKey("br")) s.boilRemaining.store(doc["br"].as<int>());

    // Watchdog nested object: {a:armed, t:tripped, lc:lastCause,
    // hs:hardStopC, ar:autoReset, ...}.
    if (doc.containsKey("wd")) {
        auto wd = doc["wd"];
        if (wd.containsKey("a"))  s.wdArmed.store(wd["a"].as<bool>());
        if (wd.containsKey("t"))  s.wdTripped.store(wd["t"].as<bool>());
        if (wd.containsKey("lc")) copyStr(s.wdCause, sizeof(s.wdCause), wd["lc"].as<const char*>());
        if (wd.containsKey("hs")) s.wdHardStopC.store(wd["hs"].as<float>());
        s.wdSupported.store(true);   // wd object present → controller has TWD plugin
    }

    // Sched: keep optimistic key names for when controller starts
    // emitting them via evt:status; harmless when absent.
    if (doc.containsKey("sa"))  s.schedActive.store(doc["sa"].as<bool>());
    if (doc.containsKey("sth")) s.schedTargetHour.store(doc["sth"].as<int>());
    if (doc.containsKey("stm")) s.schedTargetMin.store(doc["stm"].as<int>());
    if (doc.containsKey("stt")) s.schedTargetTemp.store(doc["stt"].as<float>());
    if (doc.containsKey("sv"))  s.schedVolume.store(doc["sv"].as<float>());
    if (doc.containsKey("ss"))  copyStr(s.schedStatus, sizeof(s.schedStatus), doc["ss"].as<const char*>());

    // RTC
    if (doc.containsKey("rtca")) s.rtcAvailable.store(doc["rtca"].as<bool>());
    if (doc.containsKey("rtct")) s.rtcUnix.store(doc["rtct"].as<uint32_t>());
    if (doc.containsKey("rtcn")) s.rtcNtpSynced.store(doc["rtcn"].as<bool>());

    s.generation.fetch_add(1);
}

void handleAlert(const JsonDocument& doc) {
    auto& s = app_state();
    size_t tail = s.hopTail.load();
    size_t head = s.hopHead.load();
    Serial.printf("[HOP] handleAlert called head=%u tail=%u name=\"%s\" min=%d\n",
                  (unsigned)head, (unsigned)tail,
                  doc["name"].as<const char*>() ? doc["name"].as<const char*>() : "(null)",
                  doc["min"].as<int>());
    if (tail - head >= AppState::MAX_HOPS) return;
    auto& slot = s.hops[tail % AppState::MAX_HOPS];
    copyStr(slot.name, sizeof(slot.name), doc["name"].as<const char*>());
    slot.minMark = doc["min"].as<int>();
    slot.firedAtMs = millis();
    s.hopTail.store(tail + 1);
    s.generation.fetch_add(1);
}

void onNotify(NimBLERemoteCharacteristic* /*chr*/,
              uint8_t* data, size_t len, bool /*isNotify*/) {
    // The controller's evt:status payload is ~370 bytes — well past the
    // ATT_MTU 128 = 125 byte notify limit — so NimBLE peripheral splits
    // it across 3 chunks (125+125+120). We buffer until a `}` closes the
    // top-level object and parse then. Fall back to flush on overflow or
    // on stray bytes that don't fit a JSON open/close grammar.
    static char     buf[768];
    static size_t   pos = 0;
    static int      depth = 0;
    static bool     in_str = false;
    static bool     esc = false;

    for (size_t i = 0; i < len; ++i) {
        char c = (char)data[i];
        if (pos < sizeof(buf) - 1) buf[pos++] = c;
        if (esc)            { esc = false; continue; }
        if (c == '\\' && in_str) { esc = true; continue; }
        if (c == '"')       { in_str = !in_str; continue; }
        if (in_str)         continue;
        if (c == '{')       depth++;
        else if (c == '}')  {
            if (--depth == 0 && pos > 0) {
                buf[pos] = 0;
                JsonDocument doc;
                auto err = deserializeJson(doc, buf, pos);
                if (!err) {
                    const char* tp = doc["tp"];
                    // Log every notify type so we can tell when a stray
                    // evt:boil:addition slips in and triggers HopAlert.
                    static uint16_t notify_count = 0;
                    if (tp && strcmp(tp, "evt:status") != 0) {
                        Serial.printf("[NTF] #%u tp=%s\n", notify_count, tp);
                    } else if (notify_count < 5) {
                        Serial.printf("[NTF] #%u tp=%s ct=%.2f\n",
                                      notify_count, tp ? tp : "(none)",
                                      doc["ct"].as<float>());
                    }
                    ++notify_count;
                    if (tp) {
                        if      (!strcmp(tp, "evt:status"))         handleStatus(doc);
                        else if (!strcmp(tp, "evt:boil:addition"))  handleAlert(doc);
                        else if (!strcmp(tp, "evt:boil:status")) {
                            // {tp, st, rem, total} — BoilTimerPlugin.h.
                            auto& s = app_state();
                            const char* st = doc["st"];
                            s.boilActive.store(st && strcmp(st, "stopped") != 0);
                            if (doc.containsKey("rem"))   s.boilRemaining.store(doc["rem"].as<int>());
                            if (doc.containsKey("total")) s.boilTotal.store(doc["total"].as<int>());
                            s.generation.fetch_add(1);
                        }
                    }
                } else {
                    Serial.printf("[NTF] parse err after %u bytes: %s\n",
                                  (unsigned)pos, err.c_str());
                }
                pos = 0;
                depth = 0;
            }
        }
    }
    if (pos >= sizeof(buf) - 1) {
        Serial.println("[NTF] buffer overflow, dropping");
        pos = 0; depth = 0; in_str = false; esc = false;
    }
    return;
    // evt:recipe:*, evt:sched:*, evt:watchdog:* primarily come via the
    // periodic evt:status snapshot, so we don't strictly need to parse
    // them here. Hooks can land later when push-only flows matter.
}

// Pending connect target. Set from the scan callback (which runs on
// the NimBLE host task) and consumed by a dedicated FreeRTOS task —
// calling client->connect() from inside the scan callback dead-locks
// the host task and the connect returns false immediately.
volatile bool        s_wantConnect = false;
NimBLEAddress        s_pendingAddr;

class ScanCb : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        const std::string name = dev->getName();
        Serial.printf("[BLE] scan hit name=\"%s\" addr=%s rssi=%d\n",
                      name.c_str(),
                      dev->getAddress().toString().c_str(),
                      dev->getRSSI());
        if (name == "Inversa" && !s_wantConnect) {
            Serial.println("[BLE] match — queuing connect");
            NimBLEDevice::getScan()->stop();
            s_pendingAddr = dev->getAddress();
            s_wantConnect = true;
        }
    }
};

ScanCb s_scanCb;

void restart_scan() {
    delay(200);
    NimBLEDevice::getScan()->start(0, false);
    Serial.println("[BLE] scan restarted");
}

void do_connect(const NimBLEAddress& addr) {
    if (!g_client) g_client = NimBLEDevice::createClient();
    Serial.printf("[BLE] connect() → calling for %s\n",
                  addr.toString().c_str());
    constexpr int MAX_TRIES = 3;
    bool ok = false;
    for (int t = 0; t < MAX_TRIES; ++t) {
        ok = g_client->connect(addr);
        if (ok && g_client->isConnected()) break;
        Serial.printf("[BLE] connect attempt %d failed; retrying\n", t + 1);
        delay(500);
    }
    Serial.printf("[BLE] connect() returned %s\n", ok ? "true" : "false");
    if (!ok || !g_client->isConnected()) {
        Serial.println("[BLE] connect FAILED");
        restart_scan();
        return;
    }
    g_client->updateConnParams(6, 8, 0, 400);
    Serial.println("[BLE] looking up NUS service...");
    auto* svc = g_client->getService(NUS_SERVICE_UUID);
    if (!svc) {
        Serial.println("[BLE] NUS service missing — disconnect");
        g_client->disconnect();
        restart_scan();
        return;
    }
    auto* tx = svc->getCharacteristic(NUS_TX_UUID);
    g_rx     = svc->getCharacteristic(NUS_RX_UUID);
    if (!tx || !g_rx) {
        Serial.println("[BLE] NUS chars missing — disconnect");
        g_client->disconnect();
        restart_scan();
        return;
    }
    tx->subscribe(true, &onNotify);
    Serial.println("[BLE] connected + subscribed");
    app_state().bleConnected.store(true);
    app_state().generation.fetch_add(1);
}

void ble_worker_task(void*) {
    // Consumes s_wantConnect set by the scan callback. Doing the actual
    // connect from a dedicated FreeRTOS task — not from the NimBLE host
    // task's scan callback — avoids the dead-lock that was returning
    // connect() == false immediately.
    for (;;) {
        if (s_wantConnect) {
            NimBLEAddress addr = s_pendingAddr;
            s_wantConnect = false;
            do_connect(addr);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void writeJson(const JsonDocument& doc) {
    if (!g_rx) return;
    char buf[160];
    size_t n = serializeJson(doc, buf, sizeof(buf));
    if (n == 0) return;
    g_rx->writeValue(reinterpret_cast<uint8_t*>(buf), n, true);
}

}  // namespace

void ble_init() {
    NimBLEDevice::init("Inversa-Display");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);   // max TX power, mirrors gaggimate
    NimBLEDevice::setMTU(128);
    g_client = NimBLEDevice::createClient();
    auto* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&s_scanCb, false);
    scan->setActiveScan(true);
    scan->setInterval(2000);
    scan->setWindow(100);
    scan->start(0, false);
    Serial.println("[BLE] central scan started (looking for \"Inversa\")");
    // Dedicated worker so client->connect() does NOT run on the NimBLE
    // host task. Pinned to core 0 so the main loop (LVGL render on core 1)
    // is not blocked while connect is in progress.
    xTaskCreatePinnedToCore(ble_worker_task, "ble_worker",
                            4096, nullptr, 2, nullptr, 0);
}

void ble_send(const char* json) {
    if (!g_rx || !json) return;
    g_rx->writeValue(reinterpret_cast<const uint8_t*>(json), strlen(json), true);
}

// ── Typed helpers ────────────────────────────────────────────────
static void sendType(const char* type) {
    JsonDocument d;
    d["tp"] = type;
    writeJson(d);
}

void ble_send_recipe_confirm()  { sendType("req:recipe:confirm"); }
void ble_send_recipe_pause()    { sendType("req:recipe:pause"); }
void ble_send_recipe_resume()   { sendType("req:recipe:resume"); }
void ble_send_recipe_stop()     { sendType("req:recipe:stop"); }
void ble_send_watchdog_reset()  { sendType("req:watchdog:reset"); }
void ble_send_sched_stop()      { sendType("req:sched:stop"); }

void ble_send_set_temp(float c) {
    JsonDocument d;
    // Controller's CommandHandler reads FIELD_VALUE ("v"), not "temp" —
    // wrong key here meant every Manual-mode slider release was setting
    // targetTemp to 0.0 (the doc["v"] | 0.0f default in the handler).
    d["tp"] = "req:set-temp";
    d["v"]  = c;
    writeJson(d);
}

void ble_send_heater(bool on) {
    JsonDocument d;
    d["tp"] = on ? "req:heater:on" : "req:heater:off";
    writeJson(d);
}

}}  // namespace inversa::display
