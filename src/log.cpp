

#include <Arduino.h>

#include "media.h"
#include "total_time_counter.h"
#include "logs.h"
#include "log.h"

#ifdef HAS_MEDIA
    extern SDCardState sdCardState;
#endif

void logMessage(const char* message) {

    LOG_SERIAL_L(message);
#ifdef HAS_MEDIA
    if (sdCardState.isMounted && sdCardState.isLogFileOpen) {
        sdCardState.logFile->println(message);
    }
#endif
}


void logMessages(const char* message, ...) {
    va_list args;
    va_start(args, message);


    char buffer[100];

    if(totalTimeRunning){
        formatTotalTime(buffer, 100);
        logMessage(buffer);
    }

    for(;;) {
        const char* p = va_arg(args, const char*);
        if (p == nullptr) {
            break;
        }
        logMessage(p);
    }
    logMessage("\n");

    va_end(args);
}