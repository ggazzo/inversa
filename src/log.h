
#ifndef LOG_H
#define LOG_H
#include <Arduino.h>

#include "logs.h"

void logMessage(const char *message);

void logMessages(const char *message, ...);
#endif