#include "definitions.h"
#ifndef MEDIA_H
#define MEDIA_H
#ifndef __AVR__
#include "FS.h"
#endif
#include "SD.h"


#ifdef HAS_MEDIA
struct SDCardState {
    bool isMounted;
    bool isFileOpen;
    bool isLogFileOpen;
    File *file;
    File *logFile;
};

#ifndef MAX_FILES
#define MAX_FILES 10
#endif



extern SDCardState sdCardState;
#endif


void initializeSDCard();

void checkCardInsertion();

void checkCardInsertion();
#ifdef SD_DETECT_PIN

void IRAM_ATTR ISR();
#endif

#ifdef HAS_MEDIA

void setLogFile(const char *fileName);

#endif
void handlePowerLoss();
void saveStateToPowerLoss();
void removeStateFromPowerLoss();
void recoveryFromPowerLoss();
#endif