#include "definitions.h"
#include "LittleFS.h" 
#define BLESERIAL_USE_NIMBLE true
#include <BLESerial.h>
#include <SimpleKalmanFilter.h>
SimpleKalmanFilter kfilter(2, 2, 0.01);
BLESerial<> SerialBLE;

#include <Preferences.h>

extern Preferences preferences;

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
void setup() {

  Serial.begin(9600);
  SerialBLE.begin("ESP32-BLE-Slave");

  server.addHandler(&ws);
  
  /* Attach Message Callback */
  wsPrint.onMessage([&](uint8_t *data, size_t len) {
    data[len] = '\0';    
    executeCommand(reinterpret_cast<const char *>(data), &wsPrint);
  });


  MDNS.begin("inversa");

    WiFi.mode(WIFI_AP_STA);
    preferences.begin("settings", false);
    state.kp = preferences.getFloat("kp", 2.0);
    state.ki = preferences.getFloat("ki", 5.0);
    state.kd = preferences.getFloat("kd", 1.0);
    state.pOn = preferences.getFloat("pOn", P_ON_M);
    state.time = preferences.getFloat("time", 1000);
    WiFi.begin(preferences.getString("ssid", ssid), preferences.getString("password", password));
    state.volume_liters = preferences.getFloat("volume_liters", 70);
    state.power_watts = preferences.getFloat("power_watts", 3200);
    state.hysteresis_degrees_c = preferences.getFloat("hysteresis_degrees_c", 1);
    state.hysteresis_seconds = preferences.getFloat("hysteresis_seconds", 10);
    preferences.end();


    pid.SetTunings(state.kp, state.ki, state.kd, P_ON_M);
    pid.SetSampleTime(state.time);
    

    WiFi.softAP("Inversa", "12345678");

    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.println("Connecting to WiFi..");
    }
  Serial.println("Connected to the WiFi network");
  Serial.println(WiFi.localIP());

  

  #ifdef OTA
  setupOTA();
  #endif

  #ifdef I2C
  Wire.begin();
  #endif

  configureTemperatureSensor();
  configureOutputs();


  server.begin();

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
    Serial.println(WiFi.localIP());    
}

// the loop function runs over and over again forever
extern PID pid;
void loop()
{

  mainTaskMachine.run();
  readCommandFromSerial(&Serial);
  readCommandFromSerial(&SerialBLE);

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


  {
    static const int interval = 1000;  // will send topic each 7s
    static uint32_t timer = millis() + interval;

    if (millis() > timer) {

      digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) ^ 1);

      timer += interval;
    }
  }



  #ifdef OTA
    handleOTA();
    // if (MDNS.isRunning()) MDNS.update();  // Handle MDNS
  #endif
  

  ws.cleanupClients();



    state.current_temperature_c = kfilter.updateEstimate(getTemperature());

    outputControl(state);

    setOutput(state.output_val);

}
