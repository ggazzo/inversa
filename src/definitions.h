
#ifndef STASSID
#define STASSID "***REMOVED***"
#define STAPSK "***REMOVED***"
#endif
//pid settings and gains


#ifdef ESP32
#include "definitions/wemos-esp32-C#-mini.h"
#else
#define SENSOR_PIN A0
#define SD_CS_PIN D8
#define HEATER_PIN D0

#endif



#define HAS_MEDIA
#define USE_RTC

// PID Kp: 200202.47 Ki: 170644.88 Kd: 104271.75
#define OUTPUT_MIN 0
#define OUTPUT_MAX 4095
// #define KP 1
// #define KI .8
// #define KD .8
#define KP 200202.47
#define KI 170644.88
#define KD 104271.75

#define BANG_BANG 10
#define LOOP_INTERVAL 1000
#define LOOP_INTERVAL_U_S LOOP_INTERVAL * 1000

// #define AUTO_TUNE

// #define I2C

#define REFERENCE_RESISTANCE 100000
#define NOMINAL_RESISTANCE 100000
#define NOMINAL_TEMPERATURE 25
#define B_VALUE 3950

#define OTA
#define OTA_HOSTNAME "Inversa"




#ifndef POWER_LOSS_RECOVERY_FILE
    #define POWER_LOSS_RECOVERY_FILE "/recovery.bin"
#endif
