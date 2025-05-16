#include "NimBLEDevice.h"
#include "definitions.h"
#include "LittleFS.h" 
#include "NuSerial.hpp"


#include <SimpleKalmanFilter.h>
SimpleKalmanFilter kfilter(2, 2, 0.01);


#define DEVICE_NAME "Inversa"

#include <settings.h>


#include "Arduino.h"
#include <Wire.h>

#include <WiFiUdp.h>
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

#include "outputControl.h"
#include "NTPClient.h"
WiFiUDP ntpUDP;
const long utcOffsetInSeconds = - 3 * 60 * 60;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

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

RTC_DS1307 rtc;
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

#include "getTemperature.h"
#include "setOutput.h"

MachineState state;
#ifdef HAS_MEDIA
    SDCardState sdCardState;
    char fileNames[MAX_FILES][30];
#endif

// the setup function runs once when you press reset or power the board
extern PID pid;


Settings settings;
void setup() {

  Serial.begin(115200);
  while (!Serial); // Wait for USB Serial connection


  NimBLEDevice::init(DEVICE_NAME);
  NimBLEDevice::getAdvertising()->setName(DEVICE_NAME);
  NuSerial.begin(115200);


  server.addHandler(&ws);
  
  /* Attach Message Callback */
  wsPrint.onMessage([&](uint8_t *data, size_t len) {
    data[len] = '\0';    
    executeCommand(reinterpret_cast<const char *>(data), &wsPrint);
  });

  WiFi.mode(WIFI_AP_STA);

  WiFi.softAP("Inversa", "12345678");



  #ifdef OTA
  setupOTA();
  #endif

  #ifdef I2C
  Wire.begin();
  #endif

  configureTemperatureSensor();
  configureOutputs();

  // Initialize display
  #ifdef HAS_DISPLAY
  displayManager.init();
  #endif
  // Initialize state machine with idle state
  mainTaskMachine.init(&idleState);

  server.begin();

  Serial.println("\n\nESP32-S3 Mini Starting");


  Serial.println("Starting settings");
  settings.load();

  WiFi.begin(settings.getWifiSsid(), settings.getWifiPassword(), 6);

  pid.SetTunings(settings.getKp(), settings.getKi(), settings.getKd(), P_ON_M);
  pid.SetSampleTime(settings.getTime());


  Serial.print("Wifi SSID: "); Serial.println(settings.getWifiSsid());

  if( settings.getWifiSsid().length() > 0) {
    // check if is ther any ssid in preferences
    while (WiFi.status() != WL_CONNECTED && settings.getWifiSsid().length() > 0) {
      delay(500);
      Serial.println("Connecting to WiFi: " + String(ssid));
    }
    Serial.println("Connected to the WiFi network");
    Serial.println(WiFi.localIP());
  }

  MDNS.begin("inversa");

  if (rtc.begin()) {
    if (!rtc.isrunning()) {
      timeClient.begin();
      if(timeClient.update()){
        rtc.adjust(DateTime(timeClient.getEpochTime()));
      }
      else
      {
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
      }
    }
    Serial.println("RTC is running!");
  }
  initializeSDCard();
}

// the loop function runs over and over again forever
extern PID pid;
void loop()
{
  mainTaskMachine.run();

  #ifdef LED_BUILTIN
  {
    static const int interval = 1000;  // will send topic each 7s
    static uint32_t timer = millis() + interval;
    if (millis() > timer) {
      pinMode(LED_BUILTIN, OUTPUT);
      digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) ^ 1);
      timer += interval;
    }
  }
  #endif

  #ifdef OTA
    handleOTA();
  #endif

  ws.cleanupClients();

  state.current_temperature_c = kfilter.updateEstimate(getTemperature());
  outputControl(state);
  setOutput(state.output_val);

  // Update display
  #ifdef HAS_DISPLAY
  displayManager.update(state);
  displayManager.refresh();
  #endif
}
