#include "definitions.h"
#ifdef HAS_MEDIA

#define DEBUG_ESP_CORE
#ifndef __AVR__
#include "FS.h"
#endif
#include "SD.h"
#include "SPI.h"
#include "log.h"
#ifdef USE_RTC 
#include <RTClib.h>
extern RTC_DS1307 rtc;
#endif

#include "States/stateMachine.h"
#include "media.h"
#include "state.h"
#include "command.h"
#include "total_time_counter.h"

extern char fileNames[MAX_FILES][30];
extern SDCardState sdCardState;

extern MachineState state;


extern Settings settings;


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



void setLogFile(const char* fileName) {
    if (!sdCardState.isMounted) {
        sdCardState.isLogFileOpen = false;
        return;
    }

    if (sdCardState.isLogFileOpen && sdCardState.logFile != nullptr) {
        sdCardState.logFile->close(); 
        delete sdCardState.logFile;
    }

    sdCardState.logFile = new File(std::move(SD.open(fileName, FILE_WRITE)));
    sdCardState.isLogFileOpen = true;
}

void handlePowerLoss() {
    #ifndef POWER_LOSS_PIN
    saveStateToPowerLoss();
    #endif
}


void saveStateToPowerLoss() {
#ifdef HAS_MEDIA
    if(sdCardState.isMounted && state.started){
        auto time = millis();
        File file = SD.open(POWER_LOSS_RECOVERY_FILE, FILE_WRITE);
        if(!file){
            return;
        }
        file.seek(0);


    #ifndef USE_RTC 
    unsigned long timer_current_seconds = timer.timeRemaining();
    unsigned long total_timer_count_seconds = getTotalTimeCounter();
    state.total_timer_count_seconds = total_timer_count_seconds;
    state.timer_current_seconds = timer_current_seconds;

    LOG_SERIAL_L("Saving state to power loss");
    LOG_SERIAL("current ");
    LOG_SERIAL_L(state.current);
    LOG_SERIAL("total_timer_count_seconds ");
    LOG_SERIAL_L(state.total_timer_count_seconds);
    LOG_SERIAL("timer_current_seconds ");
    LOG_SERIAL_L(state.timer_current_seconds);
    #endif
        // Write state to file


        if(timer.isFinished()) {
            LOG_SERIAL_L("Timer finished");
        }

        // Copy state to file
        file.write((uint8_t*)&state, sizeof(state));
        file.close();

        LOG_SERIAL("time ");
        LOG_SERIAL_L(millis() - time);
    }
#endif
}

void removeStateFromPowerLoss() {
#ifdef HAS_MEDIA
    if(sdCardState.isMounted){
        SD.remove(POWER_LOSS_RECOVERY_FILE);
    }
#endif
}

void recoveryFromPowerLoss() {
#ifdef HAS_MEDIA
    if(sdCardState.isMounted){
        File file = SD.open(POWER_LOSS_RECOVERY_FILE, FILE_READ);
        if(!file){
            LOG_SERIAL_L("No file to recover from");
            return;
        }

        LOG_SERIAL_L("Recovering from power loss");

        MachineState storedState;
        // Copy state from file
        file.read((uint8_t*)&storedState, sizeof(state));


        if(storedState.version != state.version){
            LOG_SERIAL_L("Invalid state");
            return;
        }

        if(strlen(storedState.log_file_name) > 0){
            LOG_SERIAL("Setting log file ");
            LOG_SERIAL_L(storedState.log_file_name);
            setLogFile(storedState.log_file_name);
        }

        if(strlen(storedState.file_name) > 0){
            LOG_SERIAL("Opening file ");
            openFile(storedState.file_name);

            if(sdCardState.isFileOpen){
                LOG_SERIAL_L("File recovered from power loss");
                LOG_SERIAL("Seeking to ");
                LOG_SERIAL_L(storedState.file_position);
                sdCardState.file->seek(storedState.file_position);
            }
        }

        #ifdef USE_RTC
        if(storedState.start_total_time_seconds) {
            LOG_SERIAL("Resuming total time counter ");
            LOG_SERIAL_L(storedState.start_total_time_seconds);
            startTotalTimeCounter(storedState.start_total_time_seconds * 1000);
        }
        #else
        if(storedState.total_timer_count_seconds) {
            LOG_SERIAL("Resuming total time counter ");
            LOG_SERIAL_L(storedState.total_timer_count_seconds);
            startTotalTimeCounter(storedState.total_timer_count_seconds * 1000);
        }
        #endif

        state = storedState;
        switch (storedState.current)
        {
        case StateType::WAIT_TEMPERATURE: {
            LOG_SERIAL("Resuming Temperature ");
            LOG_SERIAL_L(storedState.target_temperature_c);
            mainTaskMachine.setState(&waitForTemperatureState);
            break;
        }



        case StateType::PREPARING: {
            LOG_SERIAL("Resuming preparing ");

            #ifdef USE_RTC

            LOG_SERIAL_L(storedState.target_temperature_c);
            LOG_SERIAL_L(storedState.target_preparing_time_seconds);

            auto current = rtc.now().secondstime();

            if(current < storedState.target_preparing_time_seconds) {
                prepareTemperature(storedState.target_temperature_c, storedState.target_preparing_time_seconds - current, settings.getVolumeLiters(), settings.getPowerWatts());
            }
            #else

            prepare(storedState.target_temperature_c, storedState.time_seconds, storedState.volume_liters, storedState.power_watts);
            #endif

            break;
        }

        case StateType::WAIT_TIMER: {

            LOG_SERIAL("Resuming timer ");

            #ifdef USE_RTC
            LOG_SERIAL_L(storedState.target_timer_time_seconds);
            auto current = rtc.now().secondstime();

            if(current < storedState.target_timer_time_seconds) {
                timer.start(storedState.target_timer_time_seconds - current);
            }
            else {
                timer.start(0);
            }
            #else
            LOG_SERIAL_L(storedState.timer_current_seconds);
            timer.start(state.timer_current_seconds);
            #endif
            mainTaskMachine.setState(&timerState);
            break;
        }

        case StateType::WAIT_CONFIRM: {
            LOG_SERIAL("Resuming confirm ");
            LOG_SERIAL_L(storedState.confirm_message);
            mainTaskMachine.setState(&confirmState);
            break;
        }
    
        default:
            LOG_SERIAL("Unknown state ");
            LOG_SERIAL_L(storedState.current);
            break;
        }

        file.close();

        // Delete file
        LOG_SERIAL("Deleting file ");
        LOG_SERIAL_L(POWER_LOSS_RECOVERY_FILE);
        SD.remove(POWER_LOSS_RECOVERY_FILE);

        startOperation();
    }
    
#endif
}

#endif