#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../core/EventBus.h"
#include "../core/constants.h"
#include "../core/RecoveryManager.h"
#include "../models/MachineState.h"
#include "../protocol/protocol.h"
#include "BLEPlugin.h"
#include "SDCardPlugin.h"
#include "RecipePlugin.h"
#include "PIDPlugin.h"
#include "WiFiPlugin.h"
#include "OTAPlugin.h"
#include "RampPlugin.h"
#include "BrewLogPlugin.h"
#include "BoilTimerPlugin.h"

// ─── Command Handler ────────────────────────────────────────
// Routes incoming BLE JSON commands to the appropriate plugins.
// Registered as an EventBus subscriber for BLECommandReceived.

class CommandHandler {
public:
    void init(BLEPlugin* ble, SDCardPlugin* sd, RecipePlugin* recipe, PIDPlugin* pid,
              WiFiPlugin* wifi = nullptr, OTAPlugin* ota = nullptr,
              RampPlugin* ramp = nullptr, BrewLogPlugin* brewLog = nullptr,
              BoilTimerPlugin* boilTimer = nullptr) {
        _ble = ble;
        _sd = sd;
        _recipe = recipe;
        _pid = pid;
        _wifi = wifi;
        _ota = ota;
        _ramp = ramp;
        _brewLog = brewLog;
        _boilTimer = boilTimer;

        EventBus::instance().subscribe(EventType::BLECommandReceived, [this](const Event& e) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, e.stringValue);
            if (err) return;
            handle(doc);
        });

        DEBUG_PRINTLN("[CommandHandler] Initialized");
    }

    void handle(JsonDocument& doc) {
        const char* type = doc[Protocol::FIELD_TYPE] | "";
        String rid = doc[Protocol::FIELD_REQUEST_ID] | "";

        // ── Temperature ─────────────────────────────────────
        if (strcmp(type, Protocol::REQ_SET_TEMP) == 0) {
            float temp = doc[Protocol::FIELD_VALUE] | 0.0f;
            gState.targetTemp = temp;
            gState.mode = OperatingMode::Manual;
            EventBus::instance().publish(EventType::SetpointChanged, temp);
            EventBus::instance().publish(EventType::HeaterStateChanged, true);
            sendOk(rid);
        }
        // ── Heater ──────────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_HEATER_ON) == 0) {
            EventBus::instance().publish(EventType::HeaterStateChanged, true);
            gState.mode = OperatingMode::Manual;
            sendOk(rid);
        }
        else if (strcmp(type, Protocol::REQ_HEATER_OFF) == 0) {
            EventBus::instance().publish(EventType::HeaterStateChanged, false);
            gState.mode = OperatingMode::Idle;
            gState.targetTemp = 0;
            EventBus::instance().publish(EventType::SetpointChanged, 0.0f);
            sendOk(rid);
        }
        // ── Pump ────────────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_PUMP_ON) == 0) {
            EventBus::instance().publish(EventType::PumpStateChanged, true);
            sendOk(rid);
        }
        else if (strcmp(type, Protocol::REQ_PUMP_OFF) == 0) {
            EventBus::instance().publish(EventType::PumpStateChanged, false);
            sendOk(rid);
        }
        // ── PID Settings ────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_SET_PID) == 0) {
            float kp = doc["kp"] | gState.pidKp;
            float ki = doc["ki"] | gState.pidKi;
            float kd = doc["kd"] | gState.pidKd;
            if (_pid) {
                _pid->setTunings(kp, ki, kd);
            }
            sendOk(rid);
        }
        // ── Recipe: List ────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECIPE_LIST) == 0) {
            if (!_sd) { sendError(rid, "No SD card"); return; }
            auto recipes = _sd->listRecipes();
            JsonDocument res;
            res[Protocol::FIELD_TYPE] = Protocol::RES_RECIPE_LIST;
            res[Protocol::FIELD_REQUEST_ID] = rid;
            JsonArray arr = res["recipes"].to<JsonArray>();
            for (auto& r : recipes) arr.add(r);
            _ble->sendJson(res);
        }
        // ── Recipe: Load ────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECIPE_LOAD) == 0) {
            if (!_sd || !_recipe) { sendError(rid, "Not available"); return; }
            String filename = doc["file"] | "";
            if (filename.isEmpty()) { sendError(rid, "No filename"); return; }

            String content = _sd->readRecipe(filename);
            if (content.isEmpty()) { sendError(rid, "File not found"); return; }

            JsonDocument res;
            res[Protocol::FIELD_TYPE] = Protocol::RES_RECIPE_LOAD;
            res[Protocol::FIELD_REQUEST_ID] = rid;
            res["file"] = filename;
            res["content"] = content;
            _ble->sendJson(res);
        }
        // ── Recipe: Save ────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECIPE_SAVE) == 0) {
            if (!_sd) { sendError(rid, "No SD card"); return; }
            String filename = doc["file"] | "";
            String content = doc["content"] | "";
            if (filename.isEmpty()) { sendError(rid, "No filename"); return; }
            
            if (_sd->writeRecipe(filename, content)) {
                sendOk(rid);
            } else {
                sendError(rid, "Write failed");
            }
        }
        // ── Recipe: Delete ──────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECIPE_DELETE) == 0) {
            if (!_sd) { sendError(rid, "No SD card"); return; }
            String filename = doc["file"] | "";
            if (_sd->deleteRecipe(filename)) {
                sendOk(rid);
            } else {
                sendError(rid, "Delete failed");
            }
        }
        // ── Recipe: Start ───────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECIPE_START) == 0) {
            if (!_recipe || !_sd) { sendError(rid, "Not available"); return; }
            String filename = doc["file"] | "";
            if (filename.isEmpty()) { sendError(rid, "No filename"); return; }

            String content = _sd->readRecipe(filename);
            if (content.isEmpty()) { sendError(rid, "File not found"); return; }

            if (_recipe->loadRecipe(content, filename)) {
                _recipe->start();
                sendOk(rid);
            } else {
                sendError(rid, "Invalid recipe");
            }
        }
        // ── Recipe: Stop ────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECIPE_STOP) == 0) {
            if (_recipe) _recipe->stop();
            sendOk(rid);
        }
        // ── Recipe: Pause ───────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECIPE_PAUSE) == 0) {
            if (_recipe) _recipe->pause();
            sendOk(rid);
        }
        // ── Recipe: Resume ──────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECIPE_RESUME) == 0) {
            if (_recipe) _recipe->resume();
            sendOk(rid);
        }
        // ── Recipe: Confirm ─────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECIPE_CONFIRM) == 0) {
            if (_recipe) _recipe->confirm();
            sendOk(rid);
        }
        // ── Settings: Get ───────────────────────────────────
        else if (strcmp(type, Protocol::REQ_SETTINGS_GET) == 0) {
            JsonDocument res;
            res[Protocol::FIELD_TYPE] = Protocol::RES_SETTINGS;
            res[Protocol::FIELD_REQUEST_ID] = rid;
            res["kp"] = gState.pidKp;
            res["ki"] = gState.pidKi;
            res["kd"] = gState.pidKd;
            _ble->sendJson(res);
        }
        // ── Settings: Set (with NVS persistence) ────────────
        else if (strcmp(type, Protocol::REQ_SETTINGS_SET) == 0) {
            float kp = doc["kp"] | gState.pidKp;
            float ki = doc["ki"] | gState.pidKi;
            float kd = doc["kd"] | gState.pidKd;
            
            // Validate PID params
            if (kp < 0 || kp > PID_KP_MAX) { sendError(rid, "Invalid Kp"); return; }
            if (ki < 0 || ki > PID_KI_MAX) { sendError(rid, "Invalid Ki"); return; }
            if (kd < 0 || kd > PID_KD_MAX) { sendError(rid, "Invalid Kd"); return; }
            
            if (_pid) {
                _pid->setTunings(kp, ki, kd, true);  // persist = true
            }
            sendOk(rid);
        }
        // ── Info ────────────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_INFO) == 0) {
            JsonDocument res;
            res[Protocol::FIELD_TYPE] = Protocol::RES_INFO;
            res[Protocol::FIELD_REQUEST_ID] = rid;
            res["fw"] = BUILD_GIT_VERSION;
            res["build"] = BUILD_TIMESTAMP;
            res["name"] = FIRMWARE_NAME;
            res["heap"] = ESP.getFreeHeap();
            _ble->sendJson(res);
        }
        // ── Status (explicit request) ───────────────────────
        else if (strcmp(type, Protocol::REQ_STATUS) == 0) {
            // Telemetry will be sent on next cycle; send one now
            sendOk(rid);
        }
        // ── Recovery: Resume ────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECOVERY_RESUME) == 0) {
            if (!gState.hasRecoveryData) {
                sendError(rid, "No recovery data");
                return;
            }
            if (!_sd || !_recipe) {
                sendError(rid, "Not available");
                return;
            }
            
            RecoveryData recoveryData;
            if (!RecoveryManager::instance().loadRecovery(recoveryData)) {
                sendError(rid, "Failed to load recovery");
                return;
            }
            
            // Load the recipe content
            String content = _sd->readRecipe(String(recoveryData.recipeName) + ".txt");
            if (content.isEmpty()) {
                sendError(rid, "Recipe file not found");
                RecoveryManager::instance().clearRecovery();
                gState.hasRecoveryData = false;
                return;
            }
            
            // Restore recipe state
            if (_recipe->restoreFromRecovery(recoveryData, content)) {
                gState.hasRecoveryData = false;
                sendOk(rid);
            } else {
                sendError(rid, "Failed to restore recipe");
            }
        }
        // ── Recovery: Discard ───────────────────────────────
        else if (strcmp(type, Protocol::REQ_RECOVERY_DISCARD) == 0) {
            RecoveryManager::instance().clearRecovery();
            gState.hasRecoveryData = false;
            gState.recoveryRecipeName = "";
            sendOk(rid);
        }
        // ── WiFi: Configure ─────────────────────────────────
        else if (strcmp(type, Protocol::REQ_WIFI_CONFIG) == 0) {
            if (!_wifi) { sendError(rid, "WiFi not available"); return; }
            String ssid = doc["ssid"] | "";
            String pwd = doc["pwd"] | "";
            if (ssid.isEmpty()) { sendError(rid, "SSID required"); return; }
            _wifi->configure(ssid, pwd);
            sendOk(rid);
        }
        // ── WiFi: Connect ───────────────────────────────────
        else if (strcmp(type, Protocol::REQ_WIFI_CONNECT) == 0) {
            if (!_wifi) { sendError(rid, "WiFi not available"); return; }
            _wifi->connect();
            sendOk(rid);
        }
        // ── WiFi: Disconnect ────────────────────────────────
        else if (strcmp(type, Protocol::REQ_WIFI_DISCONNECT) == 0) {
            if (!_wifi) { sendError(rid, "WiFi not available"); return; }
            _wifi->disconnect();
            sendOk(rid);
        }
        // ── WiFi: Status ────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_WIFI_STATUS) == 0) {
            if (!_wifi) { sendError(rid, "WiFi not available"); return; }
            _wifi->sendWiFiStatus();
            sendOk(rid);
        }
        // ── OTA: Check ──────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_OTA_CHECK) == 0) {
            if (!_ota) { sendError(rid, "OTA not available"); return; }
            _ota->checkForUpdate();
            sendOk(rid);
        }
        // ── OTA: Install ────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_OTA_INSTALL) == 0) {
            if (!_ota) { sendError(rid, "OTA not available"); return; }
            _ota->installUpdate();
            // Note: if successful, device will restart and won't send response
            sendOk(rid);
        }
        // ── Ramp: Set Rate ──────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RAMP_SET) == 0) {
            if (!_ramp) { sendError(rid, "Ramp not available"); return; }
            float rate = doc["rate"] | 0.0f;
            if (rate < 0 || rate > 10.0f) { sendError(rid, "Invalid rate (0-10)"); return; }
            _ramp->setRate(rate);
            EventBus::instance().publish(EventType::RampConfigChanged, rate);
            sendOk(rid);
        }
        // ── Ramp: Stop ──────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RAMP_STOP) == 0) {
            if (!_ramp) { sendError(rid, "Ramp not available"); return; }
            _ramp->stopRamp();
            sendOk(rid);
        }
        // ── Brew Log: Start ─────────────────────────────────
        else if (strcmp(type, Protocol::REQ_LOG_START) == 0) {
            if (!_brewLog) { sendError(rid, "BrewLog not available"); return; }
            _brewLog->start();
            sendOk(rid);
        }
        // ── Brew Log: Stop ──────────────────────────────────
        else if (strcmp(type, Protocol::REQ_LOG_STOP) == 0) {
            if (!_brewLog) { sendError(rid, "BrewLog not available"); return; }
            _brewLog->stop();
            sendOk(rid);
        }
        // ── Brew Log: Export ────────────────────────────────
        else if (strcmp(type, Protocol::REQ_LOG_EXPORT) == 0) {
            if (!_brewLog) { sendError(rid, "BrewLog not available"); return; }
            String fmt = doc["fmt"] | "csv";
            uint16_t chunk = doc["chunk"] | 0;
            
            uint16_t total = _brewLog->getEntryCount();
            uint16_t chunkSize = (fmt == "json") ? 30 : 50;
            uint16_t totalChunks = _brewLog->getTotalChunks(chunkSize);
            
            JsonDocument res;
            res[Protocol::FIELD_TYPE] = Protocol::EVT_LOG_DATA;
            res[Protocol::FIELD_REQUEST_ID] = rid;
            res["chunk"] = chunk;
            res["total"] = totalChunks;
            res["entries"] = total;
            
            if (fmt == "json") {
                res["data"] = _brewLog->exportJSON(chunk * chunkSize, chunkSize);
            } else {
                res["data"] = _brewLog->exportCSV(chunk * chunkSize, chunkSize);
            }
            
            _ble->sendJson(res);
        }
        // ── Boil Timer: Start ───────────────────────────────
        else if (strcmp(type, Protocol::REQ_BOIL_START) == 0) {
            if (!_boilTimer) { sendError(rid, "BoilTimer not available"); return; }
            uint16_t minutes = doc["min"] | 60;
            
            // Clear previous additions and add new ones
            _boilTimer->clearAdditions();
            JsonArray additions = doc["additions"].as<JsonArray>();
            if (additions) {
                for (JsonObject add : additions) {
                    uint16_t addMin = add["min"] | 0;
                    const char* addName = add["name"] | "Addition";
                    _boilTimer->addAddition(addMin, addName);
                }
            }
            
            _boilTimer->start(minutes);
            sendOk(rid);
        }
        // ── Boil Timer: Stop ────────────────────────────────
        else if (strcmp(type, Protocol::REQ_BOIL_STOP) == 0) {
            if (!_boilTimer) { sendError(rid, "BoilTimer not available"); return; }
            _boilTimer->stop();
            sendOk(rid);
        }
        // ── Boil Timer: Pause ───────────────────────────────
        else if (strcmp(type, Protocol::REQ_BOIL_PAUSE) == 0) {
            if (!_boilTimer) { sendError(rid, "BoilTimer not available"); return; }
            _boilTimer->pause();
            sendOk(rid);
        }
        // ── Boil Timer: Resume ──────────────────────────────
        else if (strcmp(type, Protocol::REQ_BOIL_RESUME) == 0) {
            if (!_boilTimer) { sendError(rid, "BoilTimer not available"); return; }
            _boilTimer->resume();
            sendOk(rid);
        }
        // ── Boil Timer: Add Addition ────────────────────────
        else if (strcmp(type, Protocol::REQ_BOIL_ADD) == 0) {
            if (!_boilTimer) { sendError(rid, "BoilTimer not available"); return; }
            uint16_t addMin = doc["min"] | 0;
            String addName = doc["name"] | "Addition";
            if (_boilTimer->addAddition(addMin, addName.c_str())) {
                sendOk(rid);
            } else {
                sendError(rid, "Max additions reached");
            }
        }
        // ── Mash-Out: Set ───────────────────────────────────
        else if (strcmp(type, Protocol::REQ_MASHOUT_SET) == 0) {
            bool enabled = doc["enabled"] | false;
            float temp = doc["temp"] | 76.0f;
            
            if (temp < 70.0f || temp > 80.0f) {
                sendError(rid, "Temp must be 70-80C");
                return;
            }
            
            gState.mashOutEnabled = enabled;
            gState.mashOutTemp = temp;
            sendOk(rid);
        }
        else {
            sendError(rid, "Unknown command");
        }
    }

private:
    BLEPlugin* _ble = nullptr;
    SDCardPlugin* _sd = nullptr;
    RecipePlugin* _recipe = nullptr;
    PIDPlugin* _pid = nullptr;
    WiFiPlugin* _wifi = nullptr;
    OTAPlugin* _ota = nullptr;
    RampPlugin* _ramp = nullptr;
    BrewLogPlugin* _brewLog = nullptr;
    BoilTimerPlugin* _boilTimer = nullptr;

    void sendOk(const String& rid) {
        if (!_ble || rid.isEmpty()) return;
        JsonDocument doc;
        doc[Protocol::FIELD_TYPE] = Protocol::RES_OK;
        doc[Protocol::FIELD_REQUEST_ID] = rid;
        _ble->sendJson(doc);
    }

    void sendError(const String& rid, const String& msg) {
        if (!_ble) return;
        _ble->sendError(rid, msg);
    }
};
