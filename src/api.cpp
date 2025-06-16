#include "api.h"
#include "state.h"
#include "Components/Settings.h"
#include "modules.h"
#include <RTClib.h>
#include <WiFiServer.h>
#include <map>
#include <vector>

extern MachineState state;
extern Settings settings;
// BierBot* bierbot = nullptr;

#define SECONDS_FROM_1970_TO_2000 946684800
#define API_PORT 80

API::API(MainController<StateType, Steps>* ctrl) : server(API_PORT), controller(ctrl) {}

void API::stop() {
    this->initialized = false;
    this->server.stop();
}

void API::setupRoutes() {
    std::vector<RouteHandler> tempHandlers = {
        {"GET", [this](WiFiClient& client, const char* body) { this->handleGetTemperature(client); }},
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostTemperature(client, body); }}
    };
    routes["/api/temperature"] = tempHandlers;
    
    std::vector<RouteHandler> stateHandlers = {
        {"GET", [this](WiFiClient& client, const char* body) { this->handleGetState(client); }}
    };
    routes["/api/state"] = stateHandlers;
    
    std::vector<RouteHandler> prefHandlers = {
        {"GET", [this](WiFiClient& client, const char* body) { this->handleGetPreferences(client); }},
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostPreferences(client, body); }}
    };
    routes["/api/preferences"] = prefHandlers;
    
    std::vector<RouteHandler> startHandlers = {
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostStart(client); }}
    };
    routes["/api/start"] = startHandlers;
    
    std::vector<RouteHandler> abortHandlers = {
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostAbort(client); }}
    };
    routes["/api/abort"] = abortHandlers;
    
    std::vector<RouteHandler> confirmHandlers = {
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostConfirm(client); }}
    };
    routes["/api/confirm"] = confirmHandlers;
    
    std::vector<RouteHandler> waitTimerHandlers = {
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostWaitTimer(client, body); }}
    };
    routes["/api/wait_timer"] = waitTimerHandlers;
    
    std::vector<RouteHandler> waitTempHandlers = {
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostWaitTemperature(client, body); }}
    };
    routes["/api/wait_temperature"] = waitTempHandlers;
    
    std::vector<RouteHandler> prepRelHandlers = {
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostPrepareRelative(client, body); }}
    };
    routes["/api/prepare_relative"] = prepRelHandlers;
    
    std::vector<RouteHandler> prepAbsHandlers = {
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostPrepareAbsolute(client, body); }}
    };
    routes["/api/prepare_absolute"] = prepAbsHandlers;
    
    std::vector<RouteHandler> pidHandlers = {
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostPID(client, body); }}
    };
    routes["/api/pid"] = pidHandlers;
    
    #ifdef BUZZER_PIN
    std::vector<RouteHandler> beepHandlers = {
        {"POST", [this](WiFiClient& client, const char* body) { this->handlePostBeep(client); }}
    };
    routes["/api/beep"] = beepHandlers;
    #endif
    
    std::vector<RouteHandler> fileHandlers = {
        {"GET", [this](WiFiClient& client, const char* body) { this->handleGetFile(client, body); }}
    };
    routes["/api/file"] = fileHandlers;
}

void API::setup() {
    if (this->initialized) {
        server.stop();
    }
    server.begin();
    setupRoutes();
    this->initialized = true;
}

void API::loop() {
    if (!this->initialized) return;
    WiFiClient client = server.available();
    if (!client) return;

    String currentLine = "";
    String method = "";
    String path = "";
    String query = "";
    String body = "";
    bool isBody = false;
    int contentLength = 0;
    String requestLine = "";

    bool isFirstLine = true;
    while (client.connected()) {
        if (client.available()) {
            char c = client.read();

            if (c == '\n') {
                if (requestLine.length() == 0) {
                    break;
                }

                if (isFirstLine) {
                    isFirstLine = false;
                    int methodEnd = requestLine.indexOf(' ');
                    int routeEnd = requestLine.indexOf(' ', methodEnd + 1);

                    method = requestLine.substring(0, methodEnd);
                    path = requestLine.substring(methodEnd + 1, routeEnd);
                }

                requestLine = "";
            } else if (c != '\r') {
                requestLine += c;
            }
        }
    }

    auto routeIt = routes.find(path);
    if (routeIt != routes.end()) {
        for (const auto& handler : routeIt->second) {
            if (String(handler.method) == method) {
                handler.handler(client, body.c_str());
                client.stop();
                return;
            }
        }
    }
    
    sendResponse(client, 404, "application/json", "{\"error\":\"Not found\"}");
    client.stop();
}

void API::sendResponse(WiFiClient &client, int statusCode, const char* contentType, const char* body) {
    client.print("HTTP/1.1 ");
    client.print(statusCode);
    client.println(" OK");
    client.print("Content-Type: ");
    client.println(contentType);
    client.print("Content-Length: ");
    client.println(strlen(body));
    client.println();
    client.println(body);
}

void API::handleGetTemperature(WiFiClient &client) {
    JsonDocument doc;
    doc["temperature"] = state.current_temperature_c;
    doc["target_temperature"] = state.target_temperature_c;
    doc["output"] = state.output_val;
    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response.c_str());
}

void API::handlePostTemperature(WiFiClient &client, const char* body) {
    JsonDocument doc;
    deserializeJson(doc, body);
    if (doc.containsKey("temperature")) {
        float temp = doc["temperature"];
        controller->setTargetTemperature(temp);
        sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
    } else {
        sendResponse(client, 400, "application/json", "{\"error\":\"Missing temperature parameter\"}");
    }
}

void API::handleGetState(WiFiClient &client) {
    JsonDocument doc;
    doc["type"] = "sync";
    doc["temperature"] = state.current_temperature_c;
    doc["target_temperature"] = state.target_temperature_c;
    doc["output"] = constrain(map(state.output_val, 0, 255, 0, 100), 0, 100);
    doc["started"] = state.started;
    doc["time"] = settings.getTime();
    doc["started_at"] = DateTime(state.total_time_seconds_start + SECONDS_FROM_1970_TO_2000).timestamp();
    doc["target_timer_time_seconds"] = DateTime(state.target_timer_time_seconds + SECONDS_FROM_1970_TO_2000).timestamp();
    doc["step_time_seconds_start"] = DateTime(state.step_time_seconds_start + SECONDS_FROM_1970_TO_2000).timestamp();
    doc["step_time_seconds_estimated"] = DateTime(state.step_time_seconds_estimated + SECONDS_FROM_1970_TO_2000).timestamp();
    doc["state"] = controller->getState();
    doc["sd_present"] = state.sd_present;
    String response;
    serializeJson(doc, response);
    sendResponse(client, 200, "application/json", response.c_str());
}

void API::handleGetPreferences(WiFiClient &client) {
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
    sendResponse(client, 200, "application/json", response.c_str());
}

void API::handlePostPreferences(WiFiClient &client, const char* body) {
    JsonDocument doc;
    deserializeJson(doc, body);
    
    if (doc.containsKey("kp")) settings.setKp(doc["kp"]);
    if (doc.containsKey("ki")) settings.setKi(doc["ki"]);
    if (doc.containsKey("kd")) settings.setKd(doc["kd"]);
    if (doc.containsKey("pOn")) settings.setPOn(doc["pOn"]);
    if (doc.containsKey("hysteresis_degrees_c")) settings.setHysteresisDegreesC(doc["hysteresis_degrees_c"]);
    if (doc.containsKey("hysteresis_seconds")) settings.setHysteresisSeconds(doc["hysteresis_seconds"]);
    if (doc.containsKey("volume_liters")) settings.setVolumeLiters(doc["volume_liters"]);
    if (doc.containsKey("power_watts")) settings.setPowerWatts(doc["power_watts"]);
    if (doc.containsKey("wifi_ssid")) settings.setWifiSsid(doc["wifi_ssid"].as<String>());
    if (doc.containsKey("wifi_password")) settings.setWifiPassword(doc["wifi_password"].as<String>());
    
    settings.save();
    sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
}

void API::handlePostStart(WiFiClient &client) {
    controller->startTotalTimeCounter();
    sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
}

void API::handlePostAbort(WiFiClient &client) {
    controller->abort();
    sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
}

void API::handlePostConfirm(WiFiClient &client) {
    controller->confirm();
    sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
}

void API::handlePostWaitTimer(WiFiClient &client, const char* body) {
    JsonDocument doc;
    deserializeJson(doc, body);
    if (doc.containsKey("duration")) {
        unsigned long duration = doc["duration"];
        controller->waitForTimer(duration);
        sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
    } else {
        sendResponse(client, 400, "application/json", "{\"error\":\"Missing duration parameter\"}");
    }
}

void API::handlePostWaitTemperature(WiFiClient &client, const char* body) {
    JsonDocument doc;
    deserializeJson(doc, body);
    if (doc.containsKey("temperature")) {
        float temp = doc["temperature"];
        controller->setTargetTemperatureAndWait(temp);
        sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
    } else {
        sendResponse(client, 400, "application/json", "{\"error\":\"Missing temperature parameter\"}");
    }
}

void API::handlePostPrepareRelative(WiFiClient &client, const char* body) {
    JsonDocument doc;
    deserializeJson(doc, body);
    if (doc.containsKey("temperature") && doc.containsKey("minutes")) {
        float temp = doc["temperature"];
        unsigned long minutes = doc["minutes"];
        controller->setTargetTemperature(temp);
        controller->waitForTimer(minutes * 60);
        sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
    } else {
        sendResponse(client, 400, "application/json", "{\"error\":\"Missing temperature or minutes parameter\"}");
    }
}

void API::handlePostPrepareAbsolute(WiFiClient &client, const char* body) {
    JsonDocument doc;
    deserializeJson(doc, body);
    if (doc.containsKey("temperature") && doc.containsKey("time")) {
        float temp = doc["temperature"];
        const char* time = doc["time"];
        controller->setTargetTemperature(temp);
        sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
    } else {
        sendResponse(client, 400, "application/json", "{\"error\":\"Missing temperature or time parameter\"}");
    }
}

void API::handlePostPID(WiFiClient &client, const char* body) {
    JsonDocument doc;
    deserializeJson(doc, body);
    if (doc.containsKey("kp") && doc.containsKey("ki") && doc.containsKey("kd") && 
        doc.containsKey("pOn") && doc.containsKey("sampleTime")) {
        float kp = doc["kp"];
        float ki = doc["ki"];
        float kd = doc["kd"];
        float pOn = doc["pOn"];
        float sampleTime = doc["sampleTime"];
        communicationPeripherals->setPidParameters(kp, ki, kd, pOn, sampleTime);
        sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
    } else {
        sendResponse(client, 400, "application/json", "{\"error\":\"Missing PID parameters\"}");
    }
}

#ifdef BUZZER_PIN
void API::handlePostBeep(WiFiClient &client) {
    EasyBuzzer.singleBeep(300, 100);
    sendResponse(client, 200, "application/json", "{\"status\":\"ok\"}");
}
#endif

void API::handleGetFile(WiFiClient &client, const char* query) {
    char filename[32];
    if (sscanf(query, "filename=%31s", filename) == 1) {
        File file = SD.open("/" + String(filename), FILE_READ);
        if (!file) {
            sendResponse(client, 404, "application/json", "{\"error\":\"File not found\"}");
            return;
        }

        String content = file.readString();
        file.close();

        JsonDocument doc;
        doc["filename"] = filename;
        doc["content"] = content;
        String response;
        serializeJson(doc, response);
        sendResponse(client, 200, "application/json", response.c_str());
    } else {
        sendResponse(client, 400, "application/json", "{\"error\":\"Missing filename parameter\"}");
    }
}