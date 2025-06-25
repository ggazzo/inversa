#include "definitions.h"


#define DEBUG_ESP_CORE
#ifndef __AVR__
#include "FS.h"
#endif
#include "SD.h"
#include "SPI.h"
#ifdef USE_RTC 
#include <RTClib.h>
extern RTC_DS1307 rtc;
#endif

#include "States/stateMachine.h"
#include "media.h"
#include "state.h"
#include "command.h"

extern SDCardState sdCardState;

extern MachineState state;


extern Settings preferences;


void initializeSDCard() {

    #ifdef HAS_CUSTOM_SPI_PINS
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, SPI)) {
    #else
    if (!SD.begin(SD_CS_PIN)) {
    #endif
        Serial.println("Card failed, or not present");
        return;
    }
    state.sd_present = true;
    sdCardState.isMounted = true;
    
    


    Serial.println("SD card initialized successfully");
     
}

void openFile(const char* filename) {
    if (!sdCardState.isMounted) {
        sdCardState.isFileOpen = false;
        return;
    }

    if (sdCardState.file != nullptr) {
        sdCardState.file->close();
        delete sdCardState.file;
    }

    char buffer[30] = "/";
    strcat(buffer, filename);

    sdCardState.file = new File(std::move(SD.open(buffer, FILE_READ)));
    

    if (sdCardState.file) {
        sdCardState.isFileOpen = true;
    }
    else {
        sdCardState.isFileOpen = false;
    }
}

// void setLogFile(const char* fileName) {
//     if (!sdCardState.isMounted) {
//         sdCardState.isLogFileOpen = false;
//         return;
//     }

//     if (sdCardState.isLogFileOpen && sdCardState.logFile != nullptr) {
//         sdCardState.logFile->close(); 
//         delete sdCardState.logFile;
//     }

//     sdCardState.logFile = new File(std::move(SD.open(fileName, FILE_WRITE)));
//     sdCardState.isLogFileOpen = true;
// }
