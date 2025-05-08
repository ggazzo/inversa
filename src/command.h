#ifndef COMMAND_H
#define COMMAND_H
#ifndef __AVR__
#include "FS.h"
#endif
#include "SD.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#ifndef MAX_CMD_SIZE
#define MAX_CMD_SIZE 128
#endif

void openFile(const char* fileName);
void prepareTemperature(float targetTemperature_celsius, unsigned long desiredTime_minutes, float volume_liters, float power_watts);

void setTargetTemperature(float temp);
void waitUntilTemperatureReached(float temp);
// void waitUntilTemperatureReached(float temp, float timeout);
void startTimer(unsigned long duration);

void abortOperation();

void executeCommand(const char *command, Print* output);

bool readCommand(Stream *input, char *buffer, int length);

void readCommandFromSerial(Stream *input);

void readCommands(void);

void abortOperation();
void startOperation();

void setTargetTemperature(float target_temperature_c);

void waitUntilTemperatureReached(float target_temperature_c);

void startTimer(unsigned long duration);
void skipStep();

#endif