#include "total_time_counter.h"

#include "string.h"
#include "stdio.h"
#include <Arduino.h>

#include "state.h"

#ifdef USE_RTC
#include <RTClib.h>
extern RTC_DS1307 rtc;
extern MachineState state;
#endif



unsigned long totalTimeCounter = 0;
unsigned long totalTimeStart = 0;
bool totalTimeRunning = false;

void startTotalTimeCounter(unsigned long time)  {
    totalTimeStart = time;
    totalTimeRunning = true;
    #ifdef USE_RTC
    state.start_total_time_seconds = time;
    #endif
}
void startTotalTimeCounter() {
    #ifdef USE_RTC
    startTotalTimeCounter(rtc.now().secondstime());
    #else
    startTotalTimeCounter(millis());
    #endif
}

void stopTotalTimeCounter() {
    if (totalTimeRunning) {
        totalTimeCounter = getTotalTimeCounter();
        totalTimeRunning = false;
    }
}

void resetTotalTimeCounter() {
    totalTimeCounter = 0;
    totalTimeRunning = false;
}

unsigned long getTotalTimeCounter() {
    if (!totalTimeRunning) {
        return totalTimeCounter;
    }
    #ifdef USE_RTC
    return (rtc.now().secondstime() - totalTimeStart);
    #else
    return (millis() - totalTimeStart) / 1000;
    #endif
}

void formatTime(char *buffer, size_t bufferSize, long seconds) {
    unsigned int hours = seconds / 3600;
    unsigned int minutes = (seconds % 3600) / 60;
    unsigned int secs = seconds % 60;
    snprintf(buffer, bufferSize, "%02u:%02u:%02u", hours, minutes, secs);
}

void formatTotalTime(char *buffer, size_t bufferSize) {
    formatTime(buffer, bufferSize, getTotalTimeCounter());
}

