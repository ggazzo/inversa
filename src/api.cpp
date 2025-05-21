#include "api.h"
#include "state.h"
#include "Components/Settings.h"
#include "modules.h"
#include <RTClib.h>

extern MachineState state;
extern Settings settings;

#define SECONDS_FROM_1970_TO_2000 946684800

void setupAPI(AsyncWebServer *server, MainController<StateType> *controller) {
    // GET /api/temperature
    server->on("/api/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["temperature"] = state.current_temperature_c;
        doc["target_temperature"] = state.target_temperature_c;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/temperature
    server->on("/api/temperature", HTTP_POST, [controller](AsyncWebServerRequest *request) {
        if (request->hasParam("temperature", true)) {
            float temp = request->getParam("temperature", true)->value().toFloat();
            controller->setTargetTemperature(temp);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing temperature parameter\"}");
        }
    });

    // GET /api/state
    server->on("/api/state", HTTP_GET, [controller](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["type"] = "sync";
        doc["temperature"] = state.current_temperature_c;
        doc["target_temperature"] = state.target_temperature_c;
        doc["output"] = constrain(map(state.output_val, 0, 255, 0, 100), 0, 100);
        doc["started"] = state.started;
        doc["time"] = settings.getTime();
        doc["target_preparing_time"] = DateTime(state.target_preparing_time_seconds + SECONDS_FROM_1970_TO_2000).timestamp();
        doc["state"] = controller->getState();
        doc["sd_present"] = state.sd_present;
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/start
    server->on("/api/start", HTTP_POST, [](AsyncWebServerRequest *request) {
        startOperation();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // POST /api/abort
    server->on("/api/abort", HTTP_POST, [](AsyncWebServerRequest *request) {
        abortOperation();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // POST /api/confirm
    server->on("/api/confirm", HTTP_POST, [controller](AsyncWebServerRequest *request) {
        controller->confirm();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // GET /api/preferences
    server->on("/api/preferences", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["type"] = "preferences";
        doc["kp"] = settings.getKp();
        doc["ki"] = settings.getKi();
        doc["kd"] = settings.getKd();
        doc["pOn"] = settings.getPOn();
        doc["time"] = settings.getTime();
        doc["hysteresis_degrees_c"] = settings.getHysteresisDegreesC();
        doc["hysteresis_seconds"] = settings.getHysteresisSeconds();
        doc["volume_liters"] = settings.getVolumeLiters();
        doc["power_watts"] = settings.getPowerWatts();
        doc["wifi_ssid"] = settings.getWifiSsid();
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // POST /api/preferences
    server->on("/api/preferences", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("kp", true)) settings.setKp(request->getParam("kp", true)->value().toFloat());
        if (request->hasParam("ki", true)) settings.setKi(request->getParam("ki", true)->value().toFloat());
        if (request->hasParam("kd", true)) settings.setKd(request->getParam("kd", true)->value().toFloat());
        if (request->hasParam("pOn", true)) settings.setPOn(request->getParam("pOn", true)->value().toInt());
        if (request->hasParam("hysteresis_degrees_c", true)) settings.setHysteresisDegreesC(request->getParam("hysteresis_degrees_c", true)->value().toFloat());
        if (request->hasParam("hysteresis_seconds", true)) settings.setHysteresisSeconds(request->getParam("hysteresis_seconds", true)->value().toFloat());
        if (request->hasParam("volume_liters", true)) settings.setVolumeLiters(request->getParam("volume_liters", true)->value().toFloat());
        if (request->hasParam("power_watts", true)) settings.setPowerWatts(request->getParam("power_watts", true)->value().toFloat());
        if (request->hasParam("wifi_ssid", true)) settings.setWifiSsid(request->getParam("wifi_ssid", true)->value());
        if (request->hasParam("wifi_password", true)) settings.setWifiPassword(request->getParam("wifi_password", true)->value());
        settings.save();
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });

    // POST /api/wait_timer
    server->on("/api/wait_timer", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("duration", true)) {
            unsigned long duration = request->getParam("duration", true)->value().toInt();
            startTimer(duration);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing duration parameter\"}");
        }
    });

    // POST /api/wait_temperature
    server->on("/api/wait_temperature", HTTP_POST, [controller](AsyncWebServerRequest *request) {
        if (request->hasParam("temperature", true)) {
            float temp = request->getParam("temperature", true)->value().toFloat();
            controller->setTargetTemperatureAndWait(temp);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing temperature parameter\"}");
        }
    });

    // POST /api/prepare_relative
    server->on("/api/prepare_relative", HTTP_POST, [controller](AsyncWebServerRequest *request) {
        if (request->hasParam("temperature", true) && request->hasParam("minutes", true)) {
            float temp = request->getParam("temperature", true)->value().toFloat();
            unsigned long minutes = request->getParam("minutes", true)->value().toInt();
            controller->setTargetTemperature(temp);
            startTimer(minutes * 60);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing temperature or minutes parameter\"}");
        }
    });

    // POST /api/prepare_absolute
    server->on("/api/prepare_absolute", HTTP_POST, [controller](AsyncWebServerRequest *request) {
        if (request->hasParam("temperature", true) && request->hasParam("time", true)) {
            float temp = request->getParam("temperature", true)->value().toFloat();
            String time = request->getParam("time", true)->value();
            controller->setTargetTemperature(temp);
            // TODO: Implement absolute time preparation
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing temperature or time parameter\"}");
        }
    });

    // POST /api/pid
    server->on("/api/pid", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("kp", true) && request->hasParam("ki", true) && 
            request->hasParam("kd", true) && request->hasParam("pOn", true) && 
            request->hasParam("sampleTime", true)) {
            float kp = request->getParam("kp", true)->value().toFloat();
            float ki = request->getParam("ki", true)->value().toFloat();
            float kd = request->getParam("kd", true)->value().toFloat();
            float pOn = request->getParam("pOn", true)->value().toFloat();
            float sampleTime = request->getParam("sampleTime", true)->value().toFloat();
            communicationPeripherals->setPidParameters(kp, ki, kd, pOn, sampleTime);
            request->send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            request->send(400, "application/json", "{\"error\":\"Missing PID parameters\"}");
        }
    });

    // POST /api/beep
    #ifdef BUZZER_PIN
    server->on("/api/beep", HTTP_POST, [](AsyncWebServerRequest *request) {
        EasyBuzzer.singleBeep(300, 100);
        request->send(200, "application/json", "{\"status\":\"ok\"}");
    });
    #endif
} 