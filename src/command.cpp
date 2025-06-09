#include "command.h"
#include <RTClib.h>
#include <WiFi.h>
#ifndef __AVR__
#include "FS.h"
#endif
#include "SD.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "media.h"
#include "States/stateMachine.h"
#include "States/TimerState.h"
#include "logs.h"
#include "media.h"
#include "NTPClient.h"
#include "ArduinoJson.h"

#include "Components/Settings.h"
#ifndef MAX_CMD_SIZE
#define MAX_CMD_SIZE 128
#endif


#include "modules.h"

#ifdef BUZZER_PIN
#include <EasyBuzzer.h>
#endif

extern Settings settings;

#include "modules.h"

extern SDCardState sdCardState;

#ifdef USE_RTC
extern RTC_DS1307 rtc;
#endif

#include <PID_v1.h>

extern PID *pid;

char buffer[10];

File file;
#include "NuSerial.hpp"

// Simple base64 encoding
const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

String base64_encode(const uint8_t* data, size_t length) {
    String result;
    result.reserve(((length + 2) / 3) * 4);
    
    for (size_t i = 0; i < length; i += 3) {
        uint32_t octet_a = i < length ? data[i] : 0;
        uint32_t octet_b = i + 1 < length ? data[i + 1] : 0;
        uint32_t octet_c = i + 2 < length ? data[i + 2] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        result += base64_table[(triple >> 18) & 0x3F];
        result += base64_table[(triple >> 12) & 0x3F];
        result += base64_table[(triple >> 6) & 0x3F];
        result += base64_table[triple & 0x3F];
    }

    // Add padding
    switch (length % 3) {
        case 1:
            result[result.length() - 2] = '=';
            result[result.length() - 1] = '=';
            break;
        case 2:
            result[result.length() - 1] = '=';
            break;
    }

    return result;
}

void _executeCommand(const char *command, Print *output, JsonDocument *doc);

void executeCommand(const char* command, Print* output) {
    JsonDocument doc;
    // check if the command starts with number sequence
    if(command[0] == '$') {
        int id = atoi(command+1);
        if(id == 0) {
            return;
        }
        doc["id"] = id;
        //removes the first characters from the command
        command = strchr(command, ' ');
        if(command == nullptr) {
            return;
        }
        command++;
    }
    _executeCommand(command, output, &doc);
    String jsonString;
    serializeJson(doc, jsonString);
    output->print(jsonString);
    output->flush();
}

void _executeCommand(const char* command, Print* output, JsonDocument* doc) {

    /**
     * if we are idle we accept basic all commands
     * if we are in a different state, this means that some must be waiting
     * in short you can not start for another command until the current one is finished
     */

    bool isIdle = mainTaskMachine.getCurrentState() == &idleState;


    // if starts with # then it is a comment lets just ignore it
    if (command[0] == '#' && !file) {
        return;
    }

    // commands with no parameters

    if (strcmp(command, "BREAK") == 0 && !file) {
        controller->skip();
        return;
    }

    char *ptr;

    char* params = strchr(command, ' ');
    if(params != nullptr) {
        *(params++);
    }


#ifdef HAS_MEDIA

     // list files

    ptr = strstr(command, "LIST_FILES");
    if (ptr == command) {
        
        
    (* doc)["type"] = "list_files";

        JsonArray files = (*doc)["files"].to<JsonArray>();

        File listFiles = SD.open("/");
        File file = listFiles.openNextFile();
        while (file) {
            JsonObject fileObject = files.add<JsonObject>();
            fileObject["name"] = String(file.name());
            fileObject["size"] = file.size();
            file = listFiles.openNextFile();
        }
        return;
    }

    if(isIdle) {
        if (file) {
            ptr = strstr(command, "FILE_STOP");
            if (ptr == command) {
                
                file.close();
                return;
            }


            file.write(reinterpret_cast <const uint8_t*> (command), strlen(command));
            file.write('\n');
            return;
        }
        ptr = strstr(command, "READ_FILE");
        if (ptr == command) {
            (*doc)["type"] = "read_file";
            file = SD.open(params, FILE_READ);

            while (file.available()) {
                Serial.write(file.read());
            }
            file.close();

            file = SD.open(params, FILE_READ);

            if (!file) {
                (*doc)["status"] = "error";
                return;
            }
            (*doc)["status"] = "ok";
            (*doc)["file"] = file.readString();
            return;
        }


        ptr = strstr(command, "WRITE_FILE");
        if (ptr == command) {
            String jsonString;
            
            (*doc)["type"] = "write_file";
            file = SD.open(params, FILE_WRITE);
            if (!file) {
                (*doc)["status"] = "error";
                return;
            }
            (*doc)["status"] = "ok";
            return;
        }
        // start read from file
        ptr = strstr(command, "OPEN_FILE");

        if (ptr == command) {
            openFile(params);
            return;
        }

        ptr = strstr(command, "DELETE_FILE");
        if (ptr == command) {
            if(SD.remove(params)){
                (*doc)["type"] = "delete_file";
                (*doc)["status"] = "ok";
            } else {
                (*doc)["type"] = "delete_file";
                (*doc)["status"] = "error";
            }
        }
    }


#endif


    /** Blocking commands */

    if(isIdle) {
        ptr = strstr(command, "WAIT_TIMER");
        if (ptr == command) {
            unsigned long duration = atol(params);
            controller->waitForTimer(duration);
            return;
        }

        ptr = strstr(command, "WAIT_STEP_TIME");
        if (ptr == command) {
            controller->waitForStepTime();
            return;
        }

        ptr = strstr(command, "WAIT_TEMPERATURE");
        if (ptr == command) {
            controller->setTargetTemperatureAndWait(atof(params));
            return;
        }
        ptr = strstr(command, "WAIT_CONFIRM");
        if (ptr == command) {
            if(params == nullptr) {
                controller->waitConfirmation();
                return;
            }
            strcpy(state.message, params);
            controller->waitConfirmation();
            return;
        }

        // PREPARE_RELATIVE 25 60
        ptr = strstr(command, "PREPARE_RELATIVE");
        if (ptr == command) {
            float targetTemp = atof(strtok(params, " "));
            unsigned long minutes = atol(strtok(NULL, " "));
            controller->prepareTemperature(targetTemp, minutes);
            (*doc)["status"] = "ok";
            ESP_LOGI("PREPARE_RELATIVE", "Preparing for %d minutes", minutes);
            return;
        }

        // PREPARE_ABSOLUTE 45 2024/10/21T23:25:00
        ptr = strstr(command, "PREPARE_ABSOLUTE");
        if (ptr == command) {
            float targetTemp = atof(strtok(params, " "));
            char isoDate[20];
            strcpy(isoDate, strtok(NULL, " "));
            controller->prepareTemperature(targetTemp, isoDate);
            return;
        }

    }
    


    if (strcmp(command, "START") == 0) {
        controller->startTotalTimeCounter();
        return;
    }

    if (strcmp(command, "TOTAL_TIME_RESET") == 0) {
        controller->resetTotalTimeCounter();
        return;
    }

    if (strcmp(command, "TOTAL_TIME_STOP") == 0) {
        controller->stopTotalTimeCounter();
        return;
    }




    if (strcmp(command, "CONFIRM") == 0) {
        controller->confirm();
        return;
    }
    

    if (strcmp(command, "ABORT") == 0) {
        controller->abort();
        return;
    }

    if (strcmp(command, "END") == 0) {
        controller->abort();
        return;
    }

    if (strcmp(command, "GET_TEMP") == 0) {        
        (*doc)["temperature"] = state.current_temperature_c;
        (*doc)["target_temperature"] = state.target_temperature_c;
        return;
    }


    if(strcmp(command, "SAVE") == 0) {
        settings.save();
        return;
    
    }
    if(strcmp(command, "CLEAR_PREFERENCES") == 0) {
        settings.clear();
        return;
    }


    if(strcmp(command, "FIRMWARE_VERSION") == 0) {
        (*doc)["type"] = "firmware_version";
        (*doc)["version"] = FIRMWARE_NAME " " BUILD_GIT_VERSION;
        return;
    }

    if(strcmp(command, "FIRMWARE_UPDATE") == 0) {
        controller->checkForUpdates();
        return;
    }

    if(strcmp(command, "WIFI_STATUS") == 0) {
        (*doc)["wifi_ssid"] = settings.getWifiSsid();
        if (WiFi.status() == WL_CONNECTED)
        {
            (*doc)["wifi_status"] = "connected";
            (*doc)["ip"] = WiFi.localIP().toString();
        } else {
            (*doc)["wifi_status"] = "disconnected";
        }
        return;
    }

    if(strcmp(command, "PREFERENCES") == 0) {
        (*doc)["type"] = "preferences";
        (*doc)["kp"] = settings.getKp();
        (*doc)["ki"] = settings.getKi();
        (*doc)["kd"] = settings.getKd();
        (*doc)["pOn"] = settings.getPOn();
        (*doc)["time"] = settings.getTime();

        (*doc)["hysteresis_degrees_c"] = settings.getHysteresisDegreesC();
        (*doc)["hysteresis_seconds"] = settings.getHysteresisSeconds();
        (*doc)["volume_liters"] = settings.getVolumeLiters();
        (*doc)["power_watts"] = settings.getPowerWatts();
        return;
    }

    if(strcmp(command, "SET_STEP") == 0) {
        controller->setStep((Steps)atoi(params));
        return;
    }

    if (strcmp(command, "SYNC") == 0) {
        // Binary data format (24 bytes):
        // 4 bytes: current temp (float)
        // 4 bytes: target temp (float)
        // 1 byte: output (0-100)
        // 1 byte: sd present (0/1)
        // 1 byte: state
        // 1 byte: step
        // 4 bytes: started_at (uint32_t)
        // 4 bytes: estimated_time (uint32_t)
        // 4 bytes: target_timer_time_seconds (uint32_t)
        // 4 bytes: step_time_seconds_start (uint32_t)
        // 4 bytes: step_time_seconds_estimated (uint32_t)
        
        uint8_t binaryData[32];
        float current_temp = state.current_temperature_c;
        float target_temp = state.target_temperature_c;
        memcpy(binaryData, &current_temp, 4);
        memcpy(binaryData + 4, &target_temp, 4);
        
        binaryData[8] = constrain(map(state.output_val, 0, 255, 0, 100), 0, 100);
        binaryData[9] = state.sd_present ? 1 : 0;
        binaryData[10] = controller->getState();
        binaryData[11] = controller->getStep();
        
        uint32_t started_at = controller->getTimeStart() != 0 ? controller->getTimeStart() + SECONDS_FROM_1970_TO_2000 : 0;
        uint32_t estimated_time = controller->getEstimatedTime();
        uint32_t target_timer_time_seconds = state.target_timer_time_seconds > 0 ? state.target_timer_time_seconds + SECONDS_FROM_1970_TO_2000 : 0;
        uint32_t step_time_seconds_start = state.step_time_seconds_start != 0 ? state.step_time_seconds_start + SECONDS_FROM_1970_TO_2000 : 0;
        uint32_t step_time_seconds_estimated = state.step_time_seconds_estimated != 0 ? state.step_time_seconds_estimated : 0;

        
        memcpy(binaryData + 12, &started_at, 4);
        memcpy(binaryData + 16, &estimated_time, 4);
        memcpy(binaryData + 20, &target_timer_time_seconds, 4);
        memcpy(binaryData + 24, &step_time_seconds_start, 4);
        memcpy(binaryData + 28, &step_time_seconds_estimated, 4);

        // Create JSON with type and binary data
        (*doc)["type"] = "sync";
        (*doc)["data"] = base64_encode(binaryData, 32);
        return;
    }
#ifdef BUZZER_PIN
    if(strcmp(command, "BEEP") == 0) {
        EasyBuzzer.singleBeep(300, 100);
        return;
    }
    if(strcmp(command, "BEEP_STOP") == 0) {
        EasyBuzzer.singleBeep(300, 100);
        return;
    }
    if(strcmp(command, "BEEP_LOOP") == 0) {
        EasyBuzzer.beep(300, 100);
        return;
    }

#endif

    // commands with parameters

    ptr = strstr(command, "SSID");

    if (ptr == command) {
        settings.setWifiSsid(params);
        return;
    }

    ptr = strstr(command, "PASSWORD");

    if (ptr == command) {
        settings.setWifiPassword(params);
        return;
    }


// #ifdef HAS_MEDIA
//     ptr = strstr(command, "LOGFILE");
//     if (ptr == command) {
//         setLogFile(params);
//         return;
//     } 
// #endif

    ptr = strstr(command, "POWER");

    if (ptr == command) {
        communicationPeripherals->setPower(atof(params));   
        return;
    }

    ptr = strstr(command, "VOLUME");
    if (ptr == command) {
        communicationPeripherals->setVolume(atof(params));
        return;
    }

    ptr = strstr(command, "HYSTERESIS_TIME");
    if (ptr == command) {
        settings.setHysteresisSeconds(atof(params));
        return;
    }

    ptr = strstr(command, "HYSTERESIS_TEMP");
    if (ptr == command) {
        settings.setHysteresisDegreesC(atof(params));
        return;
    }

    ptr = strstr(command, "REMAINING");
    if (ptr == command) {
        controller->setEstimatedTime(atof(params));
        return;
    }

    // ptr = strstr(command, "MESSAGE");

    // if (ptr == command) {
    //     display.print_title(params);
    //     return;
    // }

    // ptr = strstr(command, "SUB_MESSAGE");

    // if (ptr == command) {
    //     display.print_subtitle(params);
    //     return;
    // }

    ptr = strstr(command, "TEMPERATURE");

    if (ptr == command) {
        controller->setTargetTemperature(atof(params));
        return;
    }

    ptr = strstr(command, "GET_TIME");
    if (ptr == command) {

        (*doc)["utc"] = controller->getTimeString();
    
        return;
    }

    ptr = strstr(command, "PID");
    // PID 980 0.05 230 1000
    if (ptr == command) {
        communicationPeripherals->setPidParameters(atof(strtok(params, " ")), atof(strtok(NULL, " ")),atof(strtok(NULL, " ")),atof(strtok(NULL, " ")), atoi(strtok(NULL, " ")));
        return;
    }

    ptr = strstr(command, "TUNING");

    if (ptr == command) {
        communicationPeripherals->startAutotune(atof(strtok(params, " ")), atoi(strtok(NULL, " ")));
        return;
    }

    ptr = strstr(command, "STOP_TUNING");

    if (ptr == command) {
        controller->stopAutotune();
        return;
    }

    ptr = strstr(command, "SET_DATE");

    if (ptr == command) {
        char isoDate[20];
        strcpy(isoDate, params);
        controller->setTime(isoDate);
        (*doc)["status"] = "ok";
    }
};

char command[MAX_CMD_SIZE];

bool readCommand(Stream* input, char* buffer, int length) {
  int index = 0;
  while (input->available() && index < length - 1) {
    char c = input->read();
    if (c == '\n' || c == '\r') {
      break;
    }
    buffer[index++] = c;
  }
  buffer[index] = '\0';
  
  return index > 0;
};

void readCommandFromSerial(Stream *input) {
    if(readCommand(input, command, sizeof(command))){
        executeCommand(command, input);
    }
}


void readCommandFromSDCard(void) {
#ifdef HAS_MEDIA
    bool isIdle = mainTaskMachine.getCurrentState() == &idleState;
    if(sdCardState.isFileOpen && isIdle){
        if(readCommand(sdCardState.file, command, sizeof(command))){
            executeCommand(command, &Serial);
            state.file_position = sdCardState.file->position();
        }
    }
#endif
}

void readFromSerials(void) {
    readCommandFromSerial(&Serial);
    readCommandFromSerial(&NuSerial);
}

void readCommands(void) {
    readFromSerials();
    readCommandFromSDCard();
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

    LOG_SERIAL("Opening file: "); 
    LOG_SERIAL_L(buffer);

    sdCardState.file = new File(std::move(SD.open(buffer, FILE_READ)));
    

    if (sdCardState.file) {
        sdCardState.isFileOpen = true;
        LOG_SERIAL_L("File opened successfully");
    }
    else {
        sdCardState.isFileOpen = false;
        LOG_SERIAL_L("File not opened");
    }

    mainTaskMachine.setState(&idleState);
}