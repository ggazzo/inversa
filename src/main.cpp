#include "NimBLEDevice.h"
#include "definitions.h"
#include "LittleFS.h" 
#include "NuSerial.hpp"
#include "api.h"

#include <Adafruit_NeoPixel.h>

// How many internal neopixels do we have? some boards have more than one!
#define NUMPIXELS        1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

#define DEVICE_NAME "Inversa"

#include <settings.h>


#include "Arduino.h"
#include <Wire.h>

#define SKETCH_VERSION "0.0.1"
#include "CountDown.h"
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
  #include <ESP8266mDNS.h>
  #include <ESP8266HTTPClient.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <AsyncTCP.h>
  #include <ESPmDNS.h>
#endif


#include <ESPAsyncWebServer.h>

#ifdef OTA
  #include "OTA.h"
#endif
#include "media.h"
#include "state.h"

#include "States/stateMachine.h"

#ifdef USE_RTC
#include <RTClib.h>

#include "ws.h"

#endif

CountDown countDown(CountDown::MINUTES);
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

WebSocketBroadcastPrint wsPrint(&ws);

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
  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::getAdvertising()->setName(DEVICE_NAME);
  NuSerial.begin(115200);


  server.addHandler(&ws);
  
  /* Attach Message Callback */
  wsPrint.onMessage([&](uint8_t *data, size_t len) {
    data[len] = '\0';    
    executeCommand(reinterpret_cast<const char *>(data), &wsPrint);
  });

  // Setup REST API endpoints
  setupAPI(&server, controller);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("Inversa", "12345678");



  #ifdef OTA
  setupOTA();
  #endif

  server.begin();

  controller->setup();

  WiFi.begin(settings.getWifiSsid(), settings.getWifiPassword(), 6);


  Serial.print("Wifi SSID: "); Serial.println(settings.getWifiSsid());

  if( settings.getWifiSsid().length() > 0) {
    // check if is ther any ssid in preferences
    while (WiFi.status() != WL_CONNECTED && settings.getWifiSsid().length() > 0) {
      delay(500);
    }
  }

  MDNS.begin("inversa");

  initializeSDCard();
}

void loop()
{
  controller->loop();

  #ifdef OTA
    handleOTA();
  #endif

  ws.cleanupClients();

  
}
