#ifndef LOGS_H
#define LOGS_H

#include <Arduino.h>

#define DEBUG

#ifdef DEBUG
#define LOG(message, device, method) device.method(message)
#else
#define LOG(message, device, method)
#endif

#define LOG_SERIAL_L(message) LOG(message, Serial, println)
#define LOG_SERIAL(message) LOG(message, Serial, print)

#endif
