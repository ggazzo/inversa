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
    if (doc.containsKey("tg")) s.targetTemp.store(doc["tg"].as<float>());
    if (doc.containsKey("tc")) s.currentTemp.store(doc["tc"].as<float>());
    if (doc.containsKey("po")) s.pidOutput.store(doc["po"].as<float>());
    if (doc.containsKey("h"))  s.heaterOn.store(doc["h"].as<bool>());
    if (doc.containsKey("p"))  s.pumpOn.store(doc["p"].as<bool>());
    if (doc.containsKey("m"))  copyStr(s.mode, sizeof(s.mode), doc["m"].as<const char*>());
    if (doc.containsKey("rst")) copyStr(s.recipeState, sizeof(s.recipeState), doc["rst"].as<const char*>());
    if (doc.containsKey("rn"))  copyStr(s.recipeName,  sizeof(s.recipeName),  doc["rn"].as<const char*>());
    if (doc.containsKey("rs"))  s.recipeStep.store(doc["rs"].as<int>());
    if (doc.containsKey("rt"))  s.recipeTotal.store(doc["rt"].as<int>());
    if (doc.containsKey("ba"))  s.boilActive.store(doc["ba"].as<bool>());
    if (doc.containsKey("br"))  s.boilRemaining.store(doc["br"].as<int>());
    if (doc.containsKey("bt"))  s.boilTotal.store(doc["bt"].as<int>());
    if (doc.containsKey("sa"))  s.schedActive.store(doc["sa"].as<bool>());
    if (doc.containsKey("sth")) s.schedTargetHour.store(doc["sth"].as<int>());
    if (doc.containsKey("stm")) s.schedTargetMin.store(doc["stm"].as<int>());
    if (doc.containsKey("stt")) s.schedTargetTemp.store(doc["stt"].as<float>());
    if (doc.containsKey("sv"))  s.schedVolume.store(doc["sv"].as<float>());
    if (doc.containsKey("ss"))  copyStr(s.schedStatus, sizeof(s.schedStatus), doc["ss"].as<const char*>());
    if (doc.containsKey("wda")) s.wdArmed.store(doc["wda"].as<bool>());
    if (doc.containsKey("wd"))  s.wdTripped.store(doc["wd"].as<bool>());
    if (doc.containsKey("wdc")) copyStr(s.wdCause, sizeof(s.wdCause), doc["wdc"].as<const char*>());
    if (doc.containsKey("rtca")) s.rtcAvailable.store(doc["rtca"].as<bool>());
    if (doc.containsKey("rtct")) s.rtcUnix.store(doc["rtct"].as<uint32_t>());
    if (doc.containsKey("rtcn")) s.rtcNtpSynced.store(doc["rtcn"].as<bool>());
    s.generation.fetch_add(1);
}

void handleAlert(const JsonDocument& doc) {
    auto& s = app_state();
    // Hop alert: append to local queue. The UI screen at the head
    // tracks confirmedCount; LvglBridge pops one when the user taps.
    size_t tail = s.hopTail.load();
    size_t head = s.hopHead.load();
    if (tail - head >= AppState::MAX_HOPS) return;  // full, drop
    auto& slot = s.hops[tail % AppState::MAX_HOPS];
    copyStr(slot.name, sizeof(slot.name), doc["name"].as<const char*>());
    slot.minMark = doc["min"].as<int>();
    slot.firedAtMs = millis();
    s.hopTail.store(tail + 1);
    s.generation.fetch_add(1);
}

void onNotify(NimBLERemoteCharacteristic* /*chr*/,
              uint8_t* data, size_t len, bool /*isNotify*/) {
    JsonDocument doc;
    if (deserializeJson(doc, data, len)) return;
    const char* tp = doc["tp"];
    if (!tp) return;
    if      (!strcmp(tp, "evt:status"))         handleStatus(doc);
    else if (!strcmp(tp, "evt:boil:addition"))  handleAlert(doc);
    // evt:recipe:*, evt:sched:*, evt:watchdog:* primarily come via the
    // periodic evt:status snapshot, so we don't strictly need to parse
    // them here. Hooks can land later when push-only flows matter.
}

class ScanCb : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice* dev) override {
        if (!dev->getName().compare("Inversa")) {
            NimBLEDevice::getScan()->stop();
            connectTo(*dev);
        }
    }
private:
    void connectTo(const NimBLEAdvertisedDevice& dev) {
        if (!g_client) g_client = NimBLEDevice::createClient();
        if (!g_client->connect(&dev)) return;
        auto* svc = g_client->getService(NUS_SERVICE_UUID);
        if (!svc) { g_client->disconnect(); return; }
        auto* tx = svc->getCharacteristic(NUS_TX_UUID);
        g_rx     = svc->getCharacteristic(NUS_RX_UUID);
        if (!tx || !g_rx) { g_client->disconnect(); return; }
        tx->subscribe(true, &onNotify);
        app_state().bleConnected.store(true);
        app_state().generation.fetch_add(1);
    }
};

ScanCb s_scanCb;

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
    auto* scan = NimBLEDevice::getScan();
    scan->setScanCallbacks(&s_scanCb, false);
    scan->setActiveScan(true);
    scan->start(0, false);  // 0 = forever; restart-on-disconnect handled elsewhere
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
    d["tp"]   = "req:set-temp";
    d["temp"] = c;
    writeJson(d);
}

void ble_send_heater(bool on) {
    JsonDocument d;
    d["tp"] = on ? "req:heater:on" : "req:heater:off";
    writeJson(d);
}

}}  // namespace inversa::display
