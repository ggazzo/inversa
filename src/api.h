#ifndef API_H
#define API_H

#include <HTTPClient.h>
#include "command.h"
#include "ArduinoJson.h"
#include "Controller/MainController.h"
#include "state.h"
#include <WiFiServer.h>

extern WiFiServer server;
extern MainController<StateType, Steps> *controller;

void setupAPI(MainController<StateType, Steps> *ctrl);
void handleAPI();

// Event handling
typedef void (*ServerEventHandler)(const char* event, const char* data);
void registerServerEventHandler(ServerEventHandler handler);
void broadcastServerEvent(const char* event, const char* data);

#endif 