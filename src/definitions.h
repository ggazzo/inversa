
#ifndef STASSID
#define STASSID "***REMOVED***"
#define STAPSK "***REMOVED***"
#endif

#ifndef FIRMWARE_NAME
#define FIRMWARE_NAME "inversa"
#endif


#ifdef ARDUINO_LOLIN_S3_MINI// check the board
    #include "definitions/wemos-esp32-S3-mini.h"
#else
    #include "definitions/wemos-esp32-C3-mini.h"
#endif



#define HAS_MEDIA
#define USE_RTC


#define KP 20
#define KI 0.01
#define KD 2000
#define LOOP_INTERVAL 1000
#define LOOP_INTERVAL_U_S LOOP_INTERVAL * 1000


#define OTA_HOSTNAME "Inversa"




#ifndef POWER_LOSS_RECOVERY_FILE
    #define POWER_LOSS_RECOVERY_FILE "/recovery.bin"
#endif
