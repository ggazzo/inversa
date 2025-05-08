// total_time_counter.h
#ifndef TOTAL_TIME_COUNTER_H
#define TOTAL_TIME_COUNTER_H

#include "string.h"
#include "stdio.h"


extern unsigned long totalTimeCounter;
extern unsigned long totalTimeStart;
extern bool totalTimeRunning;

void startTotalTimeCounter(unsigned long t_millis);
void startTotalTimeCounter();

void stopTotalTimeCounter();

void resetTotalTimeCounter();

unsigned long getTotalTimeCounter();

void formatTime(char *buffer, size_t bufferSize, long seconds);

void formatTotalTime(char *buffer, size_t bufferSize);

#endif // TOTAL_TIME_COUNTER_H
