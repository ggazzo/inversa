#include "NimBLEDevice.h"
#include "definitions.h"
#include "LittleFS.h" 
#include "NuSerial.hpp"
#include "api.h"

#define DEVICE_NAME "Inversa"

#include <settings.h>

#include "Arduino.h"
#include <Wire.h>

#define SKETCH_VERSION "0.0.1"
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266mDNS.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <ESPmDNS.h>
#endif

#include "media.h"
#include "state.h"

#include "States/stateMachine.h"

#ifdef USE_RTC
#include <RTClib.h>
// #include "ws.h"
#endif

// WebSocketBroadcastPrint wsPrint(&ws);

#ifdef HAS_DISPLAY
#include "display_manager.h"
DisplayManager displayManager;
#endif

const char* ssid = STASSID;
const char* password = STAPSK;

#include "modules.h"
#include "setOutput.h"

MachineState state;
#ifdef HAS_MEDIA
    SDCardState sdCardState;
    char fileNames[MAX_FILES][30];
#endif

void setup() {
  Serial.begin(115200);

  Serial.println("Starting setup");
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::getAdvertising()->setName(DEVICE_NAME);
  NuSerial.begin(115200);

  /* Attach Message Callback */
  // wsPrint.onMessage([&](uint8_t *data, size_t len) {
  //   data[len] = '\0';    
  //   executeCommand(reinterpret_cast<const char *>(data), &wsPrint);
  // });

  // Setup REST API

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Inversa", "12345678");
  
  setupAPI(controller);

  WiFi.begin(settings.getWifiSsid(), settings.getWifiPassword(), 6);
  WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), IPAddress(8,8,8,8)); 

  Serial.print("Wifi SSID: "); Serial.println(settings.getWifiSsid());

  if( settings.getWifiSsid().length() > 0) {
    // check if is ther any ssid in preferences
    while (WiFi.status() != WL_CONNECTED && settings.getWifiSsid().length() > 0) {
      delay(500);
    }
  }

  MDNS.begin("inversa");

  initializeSDCard();
  controller->setup();
}

void loop()
{
  controller->loop();

  handleAPI();
}
