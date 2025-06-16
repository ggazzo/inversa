#pragma once

#include <WiFiServer.h>
#include <WiFiClient.h>
#include <map>
#include <vector>
#include <functional>
#include "state.h"
#include "Controller/MainController.h"
#include "ArduinoJson.h"

struct RouteHandler {
    const char* method;
    std::function<void(WiFiClient&, const char*)> handler;
};

class API {
private:
    WiFiServer server;
    std::map<String, std::vector<RouteHandler>> routes;
    MainController<StateType, Steps>* controller;
    bool initialized = false;

    void setupRoutes();
    void sendResponse(WiFiClient &client, int statusCode, const char* contentType, const char* body);
    void handleGetTemperature(WiFiClient &client);
    void handlePostTemperature(WiFiClient &client, const char* body);
    void handleGetState(WiFiClient &client);
    void handleGetPreferences(WiFiClient &client);
    void handlePostPreferences(WiFiClient &client, const char* body);
    void handlePostStart(WiFiClient &client);
    void handlePostAbort(WiFiClient &client);
    void handlePostConfirm(WiFiClient &client);
    void handlePostWaitTimer(WiFiClient &client, const char* body);
    void handlePostWaitTemperature(WiFiClient &client, const char* body);
    void handlePostPrepareRelative(WiFiClient &client, const char* body);
    void handlePostPrepareAbsolute(WiFiClient &client, const char* body);
    void handlePostPID(WiFiClient &client, const char* body);
    #ifdef BUZZER_PIN
    void handlePostBeep(WiFiClient &client);
    #endif
    void handleGetFile(WiFiClient &client, const char* query);

public:
    API(MainController<StateType, Steps>* ctrl);
    void setup();
    void loop();
    void stop();
};

// Event handling
typedef void (*ServerEventHandler)(const char* event, const char* data);
void registerServerEventHandler(ServerEventHandler handler);
void broadcastServerEvent(const char* event, const char* data); 