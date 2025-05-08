// #include "definitions.h"
// #include "LittleFS.h" 

// #ifdef AUTO_TUNE
// #include "pidautotuner.h"
// PIDAutotuner tuner = PIDAutotuner();
// bool startTuning = false;
// #endif
// #include "Arduino.h"
// #include <Wire.h>
// #include <ESP8266WiFi.h>
// #include <ESP8266mDNS.h>
// #include <WiFiUdp.h>
// #define SKETCH_VERSION "0.0.1"
// #include <AutoPID.h>
// #include "CountDown.h"
// #if defined(ESP8266)
//   #include <ESP8266WiFi.h>
//   #include <ESPAsyncTCP.h>
// #elif defined(ESP32)
//   #include <WiFi.h>
//   #include <AsyncTCP.h>
// #endif

// #include <SPI.h>
// #include <SD.h>

// #include <ESPAsyncWebServer.h>
// #include <WebSerial.h>

// #ifdef OTA
//   #include "OTA.h"
// #endif

// #include <ESP8266HTTPClient.h>

// CountDown countDown(CountDown::MINUTES);


// AsyncWebServer server(80);


// const char* serverName = "http://192.168.31.114/update-sensor";


// const char* ssid = STASSID;
// const char* password = STAPSK;


// double temperature, setPoint = 50, outputVal;

// AutoPID myPID(&temperature, &setPoint, &outputVal, OUTPUT_MIN, OUTPUT_MAX, KP, KI, KD);


// #include "getTemperature.h"
// #include "setOutput.h"


// bool idle = true;



// // the setup function runs once when you press reset or power the board
// void setup() {

//   Serial.begin(9600);

//   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
//     request->redirect("/webserial");
//   });

//   server.begin();
  
//   WebSerial.begin(&server);
  
//   /* Attach Message Callback */
//   WebSerial.onMessage([&](uint8_t *data, size_t len) {
//     Serial.printf("Received %u bytes from WebSerial: ", len);
//     Serial.write(data, len);
//     Serial.println();
//     String d = "";
    
//     for(size_t i=0; i < len; i++){

//       if(char(data[i]) == ' '){
//         continue;
//       }
//       d += char(data[i]);
//     }

//     WebSerial.println("Received Data...");
//     WebSerial.println(d);


//     switch (d[0]) {
//     case 'I':{

//     uint32_t realSize = ESP.getFlashChipRealSize();
//     uint32_t ideSize = ESP.getFlashChipSize();
//     FlashMode_t ideMode = ESP.getFlashChipMode();

//     WebSerial.printf("Flash real id:   %08X\n", ESP.getFlashChipId());
//     WebSerial.printf("Flash real size: %u bytes\n\n", realSize);

//     WebSerial.printf("Flash ide  size: %u bytes\n", ideSize);
//     WebSerial.printf("Flash ide speed: %u Hz\n", ESP.getFlashChipSpeed());
//     WebSerial.printf("Flash ide mode:  %s\n", (ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN"));
//   break;
//     }
// #ifdef AUTO_TUNE
//     case 'A':
//       startTuning = true;
//     break;
// #endif

//     case 'T':
//       setPoint = d.substring(1).toFloat();
//       WebSerial.print("Target Temperature: "); 
//       WebSerial.println(setPoint);
//       break;
//     case 'S':
//       idle = d.substring(1) == "1";
//       WebSerial.print("Idle: ");
//       WebSerial.println(idle);
//       break;

//     case 'C':
//       countDown.start(d.substring(1).toInt());
//       break;
//     default:
//       break;
//     }
//   });




//   WiFi.mode(WIFI_AP_STA);
//   WiFi.begin(ssid, password);
//   WiFi.softAP("Inversa", "12345678");


//   #ifdef OTA
//   setupOTA();
//   #endif

//   #ifdef I2C
//   Wire.begin();
//   #endif

//   configureTemperatureSensor();
//   configureOutputs();


//   myPID.setBangBang(BANG_BANG, 1);
//   myPID.setTimeStep(LOOP_INTERVAL);



//   #ifdef AUTO_TUNE

//     // Set the target value to tune to
//     // This will depend on what you are tuning. This should be set to a value within
//     // the usual range of the setpoint. For low-inertia systems, values at the lower
//     // end of this range usually give better results. For anything else, start with a
//     // value at the middle of the range.
//     tuner.setTargetInputValue(70);

//     // Set the loop interval in microseconds
//     // This must be the same as the interval the PID control loop will run at
//     tuner.setLoopInterval(LOOP_INTERVAL_U_S);

//     // Set the output range
//     // These are the minimum and maximum possible output values of whatever you are
//     // using to control the system (Arduino analogWrite, for example, is 0-255)
//     tuner.setOutputRange(0, 4095);

//     // Set the Ziegler-Nichols tuning mode
//     // Set it to either PIDAutotuner::ZNModeBasicPID, PIDAutotuner::ZNModeLessOvershoot,
//     // or PIDAutotuner::ZNModeNoOvershoot. Defaults to ZNModeNoOvershoot as it is the
//     // safest option.
//     tuner.setZNMode(PIDAutotuner::ZNModeBasicPID);

//     #endif

// }

// // the loop function runs over and over again forever
// void loop() {


//   {
//     static const int interval = 1000;  // will send topic each 7s
//     static uint32_t timer = millis() + interval;

//     if (millis() > timer) {

//       digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) ^ 1);

//       timer += interval;
//     }
//   }



//   #ifdef OTA
//     handleOTA();
//     if (MDNS.isRunning()) MDNS.update();  // Handle MDNS
//   #endif
  

//   WebSerial.loop();



//   #ifdef AUTO_TUNE
//     // This must be called immediately before the tuning loop
//     // Must be called with the current time in microseconds
//     {
//       static bool tuning = false;
//       static unsigned long microseconds = micros();

//       if (startTuning) {
//         tuner.startTuningLoop(micros());
//         tuning = true;
//         startTuning = false;
//       }

//       if (tuning) {

//         microseconds = micros();

//            // This loop must run at the same speed as the PID control loop being tuned
//         while(micros() - microseconds < LOOP_INTERVAL_U_S) {
//             if(micros() - microseconds > 1000 * 10) {
//               handleOTA();
//             }
//             delayMicroseconds(1);
//         }
        

//         if (!tuner.isFinished()) {

//           // This loop must run at the same speed as the PID control loop being tuned
//           temperature = getTemperature();
//           // Call tunePID() with the input value and current time in microseconds
//           double output = tuner.tunePID(temperature, micros());

//           // Set the output - tunePid() will return values within the range configured
//           // by setOutputRange(). Don't change the value or the tuning results will be
//           // incorrect.
//           setOutput(output);

//           WebSerial.print("Tuning "+ String(tuner.getCycle()) + " " + String(temperature)+" C");
//         }

//         if (!tuner.isFinished()) {
//           return;
//         }

//         tuning = false;

//         // Turn the output off here.
//         setOutput(0);

//         // Get PID gains - set your PID controller's gains to these
//         double kp = tuner.getKp();
//         double ki = tuner.getKi();
//         double kd = tuner.getKd();

//         WebSerial.println("PID Kp: "+String(kp)+" Ki: "+String(ki)+" Kd: "+String(kd));
//       }
//     }
//     #endif


//   {
//     static const int interval = 1000;
//     static uint32_t timer = millis() + interval;

//       if (millis() > timer) {

//           temperature = getTemperature();

//           WebSerial.println("Temperature "+String(temperature)+" C");
//           WebSerial.println("Target "+String(setPoint)+" C");

//         timer += interval;
//       }
//   }
// }
