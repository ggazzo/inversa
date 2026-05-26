#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../core/EventBus.h"
#include "../core/NVSStorage.h"
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
#include "RTCPlugin.h"
#include "TimerPlugin.h"
#include "AutoTunePlugin.h"
#include "SchedulerPlugin.h"
#include "ThermalWatchdogPlugin.h"
#include "LossTunePlugin.h"
#include "../core/IHeaterDriver.h"

// ─── Command Handler ────────────────────────────────────────
// Routes incoming BLE JSON commands to the appropriate plugins.
// Registered as an EventBus subscriber for BLECommandReceived.

class CommandHandler {
public:
    void init(BLEPlugin* ble, SDCardPlugin* sd, RecipePlugin* recipe, PIDPlugin* pid,
              WiFiPlugin* wifi = nullptr, OTAPlugin* ota = nullptr,
              RampPlugin* ramp = nullptr, BrewLogPlugin* brewLog = nullptr,
              BoilTimerPlugin* boilTimer = nullptr, RTCPlugin* rtc = nullptr,
              TimerPlugin* timer = nullptr, AutoTunePlugin* autoTune = nullptr,
              SchedulerPlugin* scheduler = nullptr,
              ThermalWatchdogPlugin* watchdog = nullptr,
              LossTunePlugin* lossTune = nullptr,
              IHeaterDriver* heater = nullptr) {
        _ble = ble;
        _sd = sd;
        _recipe = recipe;
        _pid = pid;
        _wifi = wifi;
        _ota = ota;
        _ramp = ramp;
        _brewLog = brewLog;
        _boilTimer = boilTimer;
        _rtc = rtc;
        _timer = timer;
        _autoTune = autoTune;
        _scheduler = scheduler;
        _watchdog = watchdog;
        _lossTune = lossTune;
        _heater = heater;

        EventBus::instance().subscribe(EventType::BLECommandReceived, [this](const Event& e) {
            // Reuse the same JsonDocument for every inbound command. BLE
            // commands are serialized (NimBLE doesn't deliver overlapping
            // writes), so single-buffer reuse is safe.
            _doc.clear();
            DeserializationError err = deserializeJson(_doc, e.stringValue);
            if (err) return;
            handle(_doc);
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
            // P2 — defense in depth: SDCardPlugin also validates, but we want a
            // clean "Invalid filename" error instead of silent "File not found".
            if (!SDCardPlugin::isValidRecipeFilename(filename)) {
                sendError(rid, "Invalid filename"); return;
            }

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
            if (!SDCardPlugin::isValidRecipeFilename(filename)) {
                sendError(rid, "Invalid filename"); return;
            }

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
            if (!SDCardPlugin::isValidRecipeFilename(filename)) {
                sendError(rid, "Invalid filename"); return;
            }
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
            if (!SDCardPlugin::isValidRecipeFilename(filename)) {
                sendError(rid, "Invalid filename"); return;
            }

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
            
            // P4 — recoveryData.recipeName already includes ".txt" (v2 stores
            // the full filename). Stop concatenating, which produced ".txt.txt"
            // and broke resume.
            String content = _sd->readRecipe(String(recoveryData.recipeName));
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
            gState.recoveryRecipeName[0] = 0;
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
        // ── RTC: Get ────────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RTC_GET) == 0) {
            JsonDocument res;
            res[Protocol::FIELD_TYPE] = Protocol::EVT_RTC_STATUS;
            res[Protocol::FIELD_REQUEST_ID] = rid;
            res["avail"] = gState.rtcAvailable;
            res["ts"] = gState.rtcTimestamp;
            res["ntp"] = gState.rtcNtpSynced;
            if (_rtc && _rtc->isAvailable()) {
                res["time"] = _rtc->getTimeString();
                res["date"] = _rtc->getDateString();
            }
            _ble->sendJson(res);
        }
        // ── RTC: Set ────────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RTC_SET) == 0) {
            if (!_rtc) { sendError(rid, "RTC not available"); return; }
            
            // Accept either unix timestamp or ISO string
            if (doc["ts"].is<uint32_t>()) {
                uint32_t ts = doc["ts"] | 0;
                _rtc->setTime(ts);
                sendOk(rid);
            } else if (doc["iso"].is<const char*>()) {
                const char* iso = doc["iso"] | "";
                _rtc->setTimeFromISO(iso);
                sendOk(rid);
            } else {
                sendError(rid, "Provide ts or iso");
            }
        }
        // ── RTC: Sync NTP ───────────────────────────────────
        else if (strcmp(type, Protocol::REQ_RTC_SYNC) == 0) {
            if (!_rtc) { sendError(rid, "RTC not available"); return; }
            if (!gState.wifiConnected) { sendError(rid, "WiFi not connected"); return; }
            _rtc->syncNTP();
            sendOk(rid);
        }
        // ── RTC: Timezone Get (P14) ─────────────────────────
        else if (strcmp(type, Protocol::REQ_RTC_TZ_GET) == 0) {
            if (!_rtc) { sendError(rid, "RTC not available"); return; }
            JsonDocument res;
            res[Protocol::FIELD_TYPE] = Protocol::RES_OK;
            res[Protocol::FIELD_REQUEST_ID] = rid;
            res["min"] = _rtc->getTimezoneOffsetMin();
            _ble->sendJson(res);
        }
        // ── RTC: Timezone Set (P14) ─────────────────────────
        else if (strcmp(type, Protocol::REQ_RTC_TZ_SET) == 0) {
            if (!_rtc) { sendError(rid, "RTC not available"); return; }
            int min = doc["min"] | -181;  // sentinel for missing
            if (min < -720 || min > 840) { sendError(rid, "min out of range [-720,840]"); return; }
            if (!_rtc->setTimezoneOffsetMin((int16_t)min)) {
                sendError(rid, "Failed to set timezone");
                return;
            }
            sendOk(rid);
        }
        // ── Timer: Start ────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_TIMER_START) == 0) {
            if (!_timer) { sendError(rid, "Timer not available"); return; }
            
            // Accept seconds or minutes
            if (doc["sec"].is<uint32_t>()) {
                uint32_t sec = doc["sec"] | 0;
                if (sec == 0) { sendError(rid, "Duration must be > 0"); return; }
                _timer->start(sec);
                sendOk(rid);
            } else if (doc["min"].is<uint32_t>()) {
                uint32_t min = doc["min"] | 0;
                if (min == 0) { sendError(rid, "Duration must be > 0"); return; }
                _timer->startMinutes(min);
                sendOk(rid);
            } else {
                sendError(rid, "Provide sec or min");
            }
        }
        // ── Timer: Stop ─────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_TIMER_STOP) == 0) {
            if (!_timer) { sendError(rid, "Timer not available"); return; }
            _timer->stop();
            sendOk(rid);
        }
        // ── Timer: Pause ────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_TIMER_PAUSE) == 0) {
            if (!_timer) { sendError(rid, "Timer not available"); return; }
            _timer->pause();
            sendOk(rid);
        }
        // ── Timer: Resume ───────────────────────────────────
        else if (strcmp(type, Protocol::REQ_TIMER_RESUME) == 0) {
            if (!_timer) { sendError(rid, "Timer not available"); return; }
            _timer->resume();
            sendOk(rid);
        }
        // ── Timer: Add Time ─────────────────────────────────
        else if (strcmp(type, Protocol::REQ_TIMER_ADD) == 0) {
            if (!_timer) { sendError(rid, "Timer not available"); return; }
            int32_t sec = doc["sec"] | 0;
            _timer->addTime(sec);
            sendOk(rid);
        }
        // ── Auto-Tune: Start ────────────────────────────────
        else if (strcmp(type, Protocol::REQ_AUTOTUNE_START) == 0) {
            if (!_autoTune) { sendError(rid, "AutoTune not available"); return; }
            if (_autoTune->isRunning()) { sendError(rid, "AutoTune already running"); return; }
            
            float setpoint = doc["temp"] | 65.0f;  // Default target 65°C
            if (setpoint < 30.0f || setpoint > 100.0f) {
                sendError(rid, "Temp must be 30-100C");
                return;
            }
            
            _autoTune->start(setpoint);
            sendOk(rid);
        }
        // ── Auto-Tune: Stop ─────────────────────────────────
        else if (strcmp(type, Protocol::REQ_AUTOTUNE_STOP) == 0) {
            if (!_autoTune) { sendError(rid, "AutoTune not available"); return; }
            _autoTune->stop();
            sendOk(rid);
        }
        // ── Timer: Alarm (absolute time) ────────────────────
        else if (strcmp(type, Protocol::REQ_TIMER_ALARM) == 0) {
            if (!_timer) { sendError(rid, "Timer not available"); return; }
            
            uint8_t hour = doc["hour"] | 0;
            uint8_t minute = doc["min"] | 0;
            
            if (hour > 23 || minute > 59) {
                sendError(rid, "Invalid time");
                return;
            }
            
            if (_timer->startAt(hour, minute)) {
                sendOk(rid);
            } else {
                sendError(rid, "RTC not available");
            }
        }
        // ── Scheduler: Set ──────────────────────────────────
        else if (strcmp(type, Protocol::REQ_SCHEDULER_SET) == 0) {
            if (!_scheduler) { sendError(rid, "Scheduler not available"); return; }
            
            uint8_t hour = doc["hour"] | 0;
            uint8_t minute = doc["min"] | 0;
            float temp = doc["temp"] | 65.0f;
            float vol = doc["vol"] | 20.0f;
            
            if (hour > 23 || minute > 59) {
                sendError(rid, "Invalid time");
                return;
            }
            if (temp < 20.0f || temp > 100.0f) {
                sendError(rid, "Temp must be 20-100C");
                return;
            }
            if (vol < 1.0f || vol > 100.0f) {
                sendError(rid, "Volume must be 1-100L");
                return;
            }
            
            if (_scheduler->schedule(hour, minute, temp, vol)) {
                sendOk(rid);
            } else {
                sendError(rid, "Scheduling failed");
            }
        }
        // ── Scheduler: Stop ─────────────────────────────────
        else if (strcmp(type, Protocol::REQ_SCHEDULER_STOP) == 0) {
            if (!_scheduler) { sendError(rid, "Scheduler not available"); return; }
            _scheduler->cancel();
            sendOk(rid);
        }
        // ── Thermal Settings: Get (P13 + dual-coeff) ────────
        else if (strcmp(type, Protocol::REQ_SETTINGS_THERMAL_GET) == 0) {
            JsonDocument res;
            res[Protocol::FIELD_TYPE] = Protocol::RES_SETTINGS_THERMAL;
            res[Protocol::FIELD_REQUEST_ID] = rid;
            res["volumeL"]        = gState.volumeLiters;
            res["powerW"]         = gState.heaterPowerWatts;
            res["ambientC"]       = gState.ambientTemp;
            res["diameterM"]      = gState.vesselDiameter;
            res["lossCoeffLidOn"] = gState.heatLossCoeffLidOn;
            res["lossCoeffLidOff"]= gState.heatLossCoeffLidOff;
            res["lidState"]       = gState.lidState == LidState::ON ? "lidOn" : "lidOff";
            res["ambientSource"]  = gState.ambientSource == AmbientSource::SENSOR ? "sensor" : "manual";
            res["ambientSensorOk"]= gState.ambientSensorOk;
            res["ambientSensorC"] = gState.ambientSensorC;
            res["ambientEffectiveC"] = getEffectiveAmbient();
            res["persisted"]      = NVSStorage::instance().hasThermalParams();
            _ble->sendJson(res);
        }
        // ── Thermal Settings: Set + persist (P13 + dual-coeff) ───
        else if (strcmp(type, Protocol::REQ_SETTINGS_THERMAL_SET) == 0) {
            float volumeL    = doc["volumeL"]         | gState.volumeLiters;
            float powerW     = doc["powerW"]          | gState.heaterPowerWatts;
            float ambientC   = doc["ambientC"]        | gState.ambientTemp;
            float diameterM  = doc["diameterM"]       | gState.vesselDiameter;
            float lossOn     = doc["lossCoeffLidOn"]  | gState.heatLossCoeffLidOn;
            float lossOff    = doc["lossCoeffLidOff"] | gState.heatLossCoeffLidOff;
            const char* srcStr = doc["ambientSource"] | (gState.ambientSource == AmbientSource::SENSOR ? "sensor" : "manual");
            const char* lidStr = doc["lidState"]      | (gState.lidState == LidState::ON ? "lidOn" : "lidOff");

            if (volumeL   < 1.0f   || volumeL   > 200.0f)   { sendError(rid, "volumeL must be 1-200L"); return; }
            if (powerW    < 500.0f || powerW    > 10000.0f) { sendError(rid, "powerW must be 500-10000W"); return; }
            if (ambientC  < -10.0f || ambientC  > 50.0f)    { sendError(rid, "ambientC must be -10..50C"); return; }
            if (diameterM < 0.1f   || diameterM > 1.0f)     { sendError(rid, "diameterM must be 0.1-1.0m"); return; }
            if (lossOn  < LOSSTUNE_COEFF_MIN || lossOn  > LOSSTUNE_COEFF_MAX) { sendError(rid, "lossCoeffLidOn must be 1-50 W/m²K"); return; }
            if (lossOff < LOSSTUNE_COEFF_MIN || lossOff > LOSSTUNE_COEFF_MAX) { sendError(rid, "lossCoeffLidOff must be 1-50 W/m²K"); return; }

            AmbientSource src = (strcmp(srcStr, "sensor") == 0) ? AmbientSource::SENSOR : AmbientSource::MANUAL;
            LidState lid     = (strcmp(lidStr, "lidOff") == 0) ? LidState::OFF : LidState::ON;

            gState.volumeLiters        = volumeL;
            gState.heaterPowerWatts    = powerW;
            gState.ambientTemp         = ambientC;
            gState.vesselDiameter      = diameterM;
            gState.heatLossCoeffLidOn  = lossOn;
            gState.heatLossCoeffLidOff = lossOff;
            gState.ambientSource       = src;
            gState.lidState            = lid;
            NVSStorage::instance().saveThermalParams(
                volumeL, powerW, ambientC, diameterM,
                lossOn, lossOff, (uint8_t)src);
            sendOk(rid);
        }
        // ── Lid State: runtime selector ─────────────────────
        else if (strcmp(type, Protocol::REQ_LID_STATE_SET) == 0) {
            const char* mode = doc["mode"] | "";
            if      (strcmp(mode, "lidOn")  == 0) gState.lidState = LidState::ON;
            else if (strcmp(mode, "lidOff") == 0) gState.lidState = LidState::OFF;
            else { sendError(rid, "mode must be lidOn or lidOff"); return; }
            sendOk(rid);
        }
        // ── LossTune ────────────────────────────────────────
        else if (strcmp(type, Protocol::REQ_LOSSTUNE_START) == 0) {
            if (!_lossTune) { sendError(rid, "losstune_unavailable"); return; }
            const char* mode = doc["mode"] | "lidOn";
            LidState targetMode = (strcmp(mode, "lidOff") == 0)
                                  ? LidState::OFF : LidState::ON;
            const char* err = _lossTune->start(targetMode);
            if (err) { sendError(rid, err); return; }
            sendOk(rid);
        }
        else if (strcmp(type, Protocol::REQ_LOSSTUNE_CANCEL) == 0) {
            if (!_lossTune) { sendError(rid, "losstune_unavailable"); return; }
            _lossTune->cancel();
            sendOk(rid);
        }
        else if (strcmp(type, Protocol::REQ_LOSSTUNE_ACCEPT) == 0) {
            if (!_lossTune) { sendError(rid, "losstune_unavailable"); return; }
            if (!_lossTune->accept()) { sendError(rid, "not_in_result"); return; }
            sendOk(rid);
        }
        else if (strcmp(type, Protocol::REQ_LOSSTUNE_REJECT) == 0) {
            if (!_lossTune) { sendError(rid, "losstune_unavailable"); return; }
            if (!_lossTune->reject()) { sendError(rid, "not_in_result"); return; }
            sendOk(rid);
        }
        // ── Temperature Calibration: Get ────────────────────
        else if (strcmp(type, Protocol::REQ_SETTINGS_CAL_GET) == 0) {
            JsonDocument res;
            res[Protocol::FIELD_TYPE] = Protocol::RES_SETTINGS_CAL;
            res[Protocol::FIELD_REQUEST_ID] = rid;
            res["slope"]     = gState.tempCalSlope;
            res["offset"]    = gState.tempCalOffset;
            res["persisted"] = NVSStorage::instance().hasTempCalibration();
            _ble->sendJson(res);
        }
        // ── Temperature Calibration: Set + persist ──────────
        else if (strcmp(type, Protocol::REQ_SETTINGS_CAL_SET) == 0) {
            float slope  = doc["slope"]  | gState.tempCalSlope;
            float offset = doc["offset"] | gState.tempCalOffset;

            // Defensive ranges — keep users from masking a broken sensor.
            // Slope error budget covers NTC tolerance + reference resistor
            // tolerance. Offset covers typical wiring / probe drift.
            if (slope  < 0.8f  || slope  > 1.2f)  { sendError(rid, "slope must be 0.8-1.2");   return; }
            if (offset < -10.0f|| offset > 10.0f) { sendError(rid, "offset must be -10..10C"); return; }

            gState.tempCalSlope  = slope;
            gState.tempCalOffset = offset;
            NVSStorage::instance().saveTempCalibration(slope, offset);
            sendOk(rid);
        }
        // ── Thermal Watchdog (001-thermal-watchdog) ─────────
        else if (strcmp(type, Protocol::REQ_WATCHDOG_RESET) == 0) {
            if (!_watchdog) { sendError(rid, "wd_unavailable"); return; }
            auto r = _watchdog->requestReset();
            if (r == ThermalWatchdogPlugin::ResetResult::Accepted) {
                sendOk(rid);
            } else if (r == ThermalWatchdogPlugin::ResetResult::NotTripped) {
                sendError(rid, "wd_not_tripped");
            } else {
                sendError(rid, "wd_still_unsafe");
            }
        }
        else if (strcmp(type, Protocol::REQ_WATCHDOG_CONFIG) == 0) {
            if (!_watchdog) { sendError(rid, "wd_unavailable"); return; }
            // Start from current config; overlay only the fields present in
            // the request (partial updates allowed, all-or-nothing on validation).
            auto cfg = _watchdog->getConfig();
            if (doc["hard_stop"].is<float>())          cfg.hardStopC       = doc["hard_stop"].as<float>();
            if (doc["sensor_fault_ms"].is<uint32_t>()) cfg.sensorFaultMs   = doc["sensor_fault_ms"].as<uint32_t>();
            if (doc["loop_stuck_ms"].is<uint32_t>())   cfg.loopStuckMs     = doc["loop_stuck_ms"].as<uint32_t>();
            if (doc["grad_factor"].is<int>())          cfg.gradFactor      = (uint8_t)doc["grad_factor"].as<int>();
            if (doc["grad_window"].is<int>())          cfg.gradWindow      = (uint8_t)doc["grad_window"].as<int>();
            if (doc["grad_floor"].is<float>())         cfg.gradFloorC      = doc["grad_floor"].as<float>();
            if (doc["safe_autoreset_c"].is<float>())   cfg.safeAutoresetC  = doc["safe_autoreset_c"].as<float>();
            if (doc["cool_min_ms"].is<uint32_t>())     cfg.coolMinMs       = doc["cool_min_ms"].as<uint32_t>();
            if (doc["auto_reset_enabled"].is<bool>())  cfg.autoResetEnabled= doc["auto_reset_enabled"].as<bool>();

            const char* badField = nullptr;
            if (!ThermalWatchdogPlugin::validateConfig(cfg, &badField)) {
                String msg = String("wd_range:") + (badField ? badField : "?");
                sendError(rid, msg); return;
            }
            _watchdog->setConfig(cfg);
            sendOk(rid);
        }
        // ── Heater driver config (burst-fire / ZC) ──────────
        // Partial update: { freq?: 50|60, burst_window?: uint8 }.
        // Both fields are validated and persisted to NVS. burst_window
        // applies at runtime; freq requires reboot to take effect (it
        // recomputes ZeroCrossDetector timings only at begin()).
        else if (strcmp(type, Protocol::REQ_HEATER_CONFIG) == 0) {
            bool hasFreq   = doc["freq"].is<int>();
            bool hasWindow = doc["burst_window"].is<int>();
            if (!hasFreq && !hasWindow) { sendError(rid, "empty_config"); return; }

            uint16_t freq   = hasFreq   ? (uint16_t)doc["freq"].as<int>()         : 0;
            uint8_t  window = hasWindow ? (uint8_t) doc["burst_window"].as<int>() : 0;

            if (hasFreq && freq != 50 && freq != 60) {
                sendError(rid, "freq_range"); return;
            }
            if (hasWindow && window < 10) {
                sendError(rid, "burst_window_range"); return;
            }

            auto& nvs = NVSStorage::instance();
            if (hasFreq)   nvs.saveHeaterMainsFreqHz(freq);
            if (hasWindow) nvs.saveHeaterBurstWindow(window);

            // Runtime apply for burst_window (no-op on soft-PWM driver).
            if (hasWindow && _heater) _heater->setBurstWindow(window);

            sendOk(rid);
        }
        // ── Device rename ────────────────────────────────────
        // { "name": "MyBrewer" } sets the user-visible BLE adv name and
        // mDNS hostname. Persists to NVS and reboots so the new label
        // is on the air the next time the device comes up. Empty name
        // clears the override and reverts to BLE_DEVICE_NAME.
        else if (strcmp(type, Protocol::REQ_DEVICE_RENAME) == 0) {
            String name = doc["name"] | "";
            if (name.length() > 31) { sendError(rid, "name_too_long"); return; }
            for (size_t i = 0; i < name.length(); ++i) {
                char c = name[i];
                bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_';
                if (!ok) { sendError(rid, "name_invalid_char"); return; }
            }
            NVSStorage::instance().saveDeviceName(name);
            sendOk(rid);
            // Tiny delay so the notify makes it onto the air before the
            // radio shuts down for the restart.
            delay(200);
            ESP.restart();
        }
        // ── Factory Reset (P7, guarded) ─────────────────────
        // Requires {confirm: "ERASE_ALL"} to wipe the NVS namespace.
        else if (strcmp(type, Protocol::REQ_FACTORY_RESET) == 0) {
            String confirm = doc["confirm"] | "";
            if (!NVSStorage::instance().factoryReset(confirm)) {
                sendError(rid, "Missing confirm: \"ERASE_ALL\""); return;
            }
            sendOk(rid);
            // Device should be rebooted by the user; we don't auto-restart so
            // the cervejeiro can decide.
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
    RTCPlugin* _rtc = nullptr;
    TimerPlugin* _timer = nullptr;
    AutoTunePlugin* _autoTune = nullptr;
    SchedulerPlugin* _scheduler = nullptr;
    ThermalWatchdogPlugin* _watchdog = nullptr;
    LossTunePlugin* _lossTune = nullptr;
    IHeaterDriver* _heater = nullptr;

    void sendOk(const String& rid) {
        if (!_ble || rid.isEmpty()) return;
        _replyDoc.clear();
        _replyDoc[Protocol::FIELD_TYPE] = Protocol::RES_OK;
        _replyDoc[Protocol::FIELD_REQUEST_ID] = rid;
        _ble->sendJson(_replyDoc);
    }

    void sendError(const String& rid, const String& msg) {
        if (!_ble) return;
        _ble->sendError(rid, msg);
    }

    // Reused docs — _doc is for inbound deserialization, _replyDoc for
    // outbound sendOk. handle() runs on the EventBus thread, never
    // reentrant, so single-buffer reuse is safe.
    JsonDocument _doc;
    JsonDocument _replyDoc;
};
