// reminder.h
#pragma once

#include "string.h"
#include <Arduino.h>

extern unsigned long millis();

#ifndef MAX_CURRENT_TIMERS
    #define MAX_CURRENT_TIMERS 10
#endif

#ifndef REMINDER_MESSAGE_SIZE
    #define REMINDER_MESSAGE_SIZE 20
#endif


struct Timer {
    unsigned long duration = 0;
    unsigned long startTime = 0;
    bool running = false;



    void replaceTimeRemaining(unsigned long timeRemaining_minutes) {
        unsigned long remaining = timeRemaining();


        if (timeRemaining_minutes == 0) {
            return;
        }

        if(timeRemaining_minutes < 0) {
            timeRemaining_minutes = 0;
        }

        startTime += (timeRemaining_minutes * 60 * 1000) - remaining;
    }
    void start(unsigned long durationInSeconds) {
        duration = durationInSeconds * 1000;
        startTime = millis();
        running = true;
    }


    void stop() {
        running = false;
    }

    unsigned long elapsed() {
        if (running) {
            return millis() - startTime;
        } else {
            return 0;
        }
    }

    bool isFinished() {
        if (running && (millis() - startTime >= duration)) {
            running = false;
            return true;
        }
        return false;
    }

    void reset() {
        running = false;
        startTime = 0;
        duration = 0;
    }


    unsigned long timeRemaining() {
        if (running) {
            return (duration - (millis() - startTime)) / 1000;
        } else {
            return 0;
        }
    }

    void formatTime(char *buffer, size_t bufferSize) {
        unsigned long totalSeconds = timeRemaining();

        unsigned int hours = totalSeconds / 3600;
        unsigned int minutes = (totalSeconds % 3600) / 60;
        unsigned int secs = totalSeconds % 60;
        snprintf(buffer, bufferSize, "%02u:%02u:%02u", hours, minutes, secs);
    }

};


extern Timer timer;