#ifndef API_H
#define API_H

#include <ESPAsyncWebServer.h>
#include "command.h"
#include "ArduinoJson.h"
#include "Controller/MainController.h"
#include "state.h"

void setupAPI(AsyncWebServer *server, MainController<StateType> *controller);

#endif 