#include "command.h"
#include <RTClib.h>
#ifndef __AVR__
#include "FS.h"
#endif
#include "SD.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include "media.h"
#include "timer.h"
#include "total_time_counter.h"
#include "States/stateMachine.h"
#include "States/TimerState.h"
#include "logs.h"
#include "log.h"
#include "estimated.h"
#include "media.h"
#include "NTPClient.h"
#include "ArduinoJson.h"

#include <Preferences.h>
#ifndef MAX_CMD_SIZE
#define MAX_CMD_SIZE 128
#endif

#ifdef BUZZER_PIN
#include <EasyBuzzer.h>
#endif

Preferences preferences;


extern PreparingStateMachine preparingState;
extern WaitForTimerStateMachine timerState;
extern SDCardState sdCardState;

#ifdef USE_RTC
extern RTC_DS1307 rtc;
#endif

#include <PID_v1.h>
extern PID pid;

#include <PID_AutoTune_v0.h>
extern PID_ATune pid_atune;

char buffer[10];

File file;

void startAutoTune();

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

    // if starts with # then it is a comment lets just ignore it
    if (command[0] == '#' && !file) {
        return;
    }

    // commands with no parameters

    if (strcmp(command, "BREAK") == 0 && !file) {
        skipStep();
        return;
    }

    char *ptr;

    char* params = strchr(command, ' ');
    if(params != nullptr) {
        *(params++);
    }

    // start write to file

#ifdef HAS_MEDIA
    // stop write to file

    ptr = strstr(command, "FILE_STOP");
    if (ptr == command) {
        
        file.close();
        return;
    }


    if (file) {
        file.write(reinterpret_cast <const uint8_t*> (command), strlen(command));
        file.write('\n');
        return;
    }

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
#endif


    /** Blocking commands */
    
    ptr = strstr(command, "WAIT_TIMER");
    if (ptr == command) {
        unsigned long duration = atol(params);
        startTimer(duration);
        return;
    }
    ptr = strstr(command, "WAIT_TEMPERATURE");
    if (ptr == command) {
        waitUntilTemperatureReached(atof(params));
        return;
    }



    logMessage(command);

    ptr = strstr(command, "ENTER_CONFIRM");
    if (ptr == command) {
        if(params == nullptr) {
            mainTaskMachine.setState(&confirmState);
            return;
        }
        strcpy(state.confirm_message, params);
        mainTaskMachine.setState(&confirmState);
        return;
    }


    if (strcmp(command, "TOTAL_TIME_START") == 0) {
        startTotalTimeCounter();
        return;
    }

    if (strcmp(command, "TOTAL_TIME_RESET") == 0) {
        resetTotalTimeCounter();
    }

    if (strcmp(command, "TOTAL_TIME_STOP") == 0) {
        stopTotalTimeCounter();
        return;
    }


    if (strcmp(command, "START") == 0) {
        startOperation();
        return;
    }
    if (strcmp(command, "ABORT") == 0) {
        abortOperation();
        return;
    }

    if (strcmp(command, "END") == 0) {
        abortOperation();
        return;
    }

    if (strcmp(command, "GET_TEMP") == 0) {        
        (*doc)["temperature"] = state.current_temperature_c;
        (*doc)["target_temperature"] = state.target_temperature_c;
        return;
    }


    if(strcmp(command, "SAVE") == 0) {

        preferences.begin("settings", false);
        preferences.putFloat("kp", state.kp);
        preferences.putFloat("ki", state.ki);
        preferences.putFloat("kd", state.kd);
        preferences.putFloat("pOn", state.pOn);
        preferences.putFloat("volume_liters", state.volume_liters);
        preferences.putFloat("power_watts", state.power_watts);
        preferences.putFloat("hysteresis_degrees_c", state.hysteresis_degrees_c);
        preferences.putFloat("hysteresis_seconds", state.hysteresis_seconds);
        preferences.end();

        return;
    
    }
    if (strcmp(command, "SYNC") == 0) {
        (*doc)["type"] = "sync";

        (*doc)["temperature"] = state.current_temperature_c;
        (*doc)["target_temperature"] = state.target_temperature_c;
        (*doc)["output"] = constrain(map(state.output_val, 0, 255, 0, 100), 0, 100);
        (*doc)["started"] = state.started;
        (*doc)["kp"] = state.kp;
        (*doc)["ki"] = state.ki;
        (*doc)["kd"] = state.kd;
        (*doc)["time"] = state.time;
        (*doc)["hysteresis_degrees_c"] = state.hysteresis_degrees_c;
        (*doc)["hysteresis_seconds"] = state.hysteresis_seconds;
        (*doc)["volume_liters"] = state.volume_liters;
        (*doc)["power_watts"] = state.power_watts;
        (*doc)["target_preparing_time"] = DateTime(state.target_preparing_time_seconds + SECONDS_FROM_1970_TO_2000).timestamp();

        (*doc)["confirm_message"] = state.confirm_message;
        (*doc)["message"] = state.message;
        (*doc)["message_line_2"] = state.message_line_2;
        (*doc)["message_line_3"] = state.message_line_3;
        (*doc)["state"] = state.current;
        (*doc)["sd_present"] = state.sd_present;
        (*doc)["tuning"] = state.tuning;

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
        preferences.begin("settings", false);
        preferences.putString("ssid", params);
        preferences.end();
        return;
    }

    ptr = strstr(command, "PASSWORD");

    if (ptr == command) {
        preferences.begin("settings", false);
        preferences.putString("password", params);
        preferences.end();
        return;
    }

    ptr = strstr(command, "TOTAL_TIME_MILESTONE");
    if (ptr == command) {
        formatTotalTime(buffer, sizeof(buffer));
        output->print(buffer);
         if(params != nullptr) {
            output->print(" ");
            output->println(params);
        } else {
            output->println();
        }
        return;
    }

#ifdef HAS_MEDIA
    ptr = strstr(command, "LOGFILE");
    if (ptr == command) {
        setLogFile(params);
        return;
    } 
#endif

    ptr = strstr(command, "POWER");

    if (ptr == command) {
        state.power_watts = atof(params);
        return;
    }

    ptr = strstr(command, "VOLUME");
    if (ptr == command) {
        state.volume_liters = atof(params);
        return;
    }

    ptr = strstr(command, "HYSTERESIS_TIME");
    if (ptr == command) {
        state.hysteresis_seconds = atof(params);
        return;
    }

    ptr = strstr(command, "HYSTERESIS_TEMP");
    if (ptr == command) {
        state.hysteresis_degrees_c = atof(params);
        return;
    }


    ptr = strstr(command, "REMAINING");
    if (ptr == command) {
        setEstimatedTime(atof(params));
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
        setTargetTemperature(atof(params));
        return;
    }

    ptr = strstr(command, "GET_TIME");
    if (ptr == command) {

        DateTime now = rtc.now();

        (*doc)["utc"] = now.timestamp();;
    
        return;
    }

    ptr = strstr(command, "PID");
    // PID 980 0.05 230 1000
    if (ptr == command) {
        float kp = atof(strtok(params, " "));
        float ki = atof(strtok(NULL, " "));
        float kd = atof(strtok(NULL, " "));
        float pOn = atof(strtok(NULL, " "));
        float time = atoi(strtok(NULL, " "));
        state.kd = kd;
        state.ki = ki;
        state.kp = kp;
        state.pOn = pOn;
        pid.SetTunings(kp, ki, kd, pOn);
        state.time = time;
        pid.SetSampleTime(time);
        return;
    }

    ptr = strstr(command, "TUNING");

    if (ptr == command) {
        startAutoTune();
        return;
    }

    ptr = strstr(command, "PREPARE_RELATIVE");

    if (ptr == command) {

        output->print("PREPARE_RELATIVE params: ");
        output->println(params);

        float targetTemp = atof(strtok(params, " "));
        unsigned long desiredTimeHours = atol(strtok(NULL, " "));
        prepareTemperature(targetTemp, desiredTimeHours, state.volume_liters, state.power_watts);
    }

    ptr = strstr(command, "PREPARE_ABSOLUTE");

    // PREPARE_ABSOLUTE 45 2024/10/21T23:25:00

    // PREPARE_RELATIVE 25 60
    if (ptr == command) {

        
        
        float targetTemp = atof(strtok(params, " "));
        char isoDate[20];
        strcpy(isoDate, strtok(NULL, " "));
        

        
        prepareTemperature(targetTemp, (DateTime(isoDate).secondstime() - rtc.now().secondstime())/ 60, state.volume_liters, state.power_watts);
    }

    ptr = strstr(command, "SET_DATE");

    if (ptr == command) {
        char isoDate[20];
        strcpy(isoDate, params);
        rtc.adjust(DateTime(isoDate));
    }

    ptr = strstr(command, "OPEN_FILE");

    if (ptr == command) {
        openFile(params);
    }

    ptr = strstr(command, "CONFIRM");
    if (ptr == command) {
        strcpy(state.confirm_message, params);
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
    if(sdCardState.isFileOpen){
        if(readCommand(sdCardState.file, command, sizeof(command))){
            executeCommand(command, &Serial);
            state.file_position = sdCardState.file->position();
        }
    }
#endif
}

void readCommands(void) {
    readCommandFromSerial(&Serial);
    readCommandFromSDCard();
}

void startOperation() {
    state.started = true;
};

void abortOperation()
{
    timer.stop();
    stopTotalTimeCounter();
    setEstimatedTime(0);
    // display.clear();
    // display.print_subtitle("");
    state.started = false;
    removeStateFromPowerLoss();
    ESP.restart();
}

void prepareTemperature(float targetTemperature_celsius, unsigned long desiredTime_minutes, float volume_liters, float power_watts) {
    preparingState.volume_liters = volume_liters;
    preparingState.power_watts = power_watts;
    preparingState.target_temperature_c = targetTemperature_celsius;

    preparingState.time_seconds = desiredTime_minutes * 60;
    Serial.println(desiredTime_minutes);
    Serial.println(preparingState.time_seconds);

    #ifdef USE_RTC 
        state.target_preparing_time_seconds = rtc.now().secondstime() + preparingState.time_seconds;
    #endif


    mainTaskMachine.setState(&preparingState);
}

void setTargetTemperature(float target_temperature_c) {
    state.target_temperature_c = target_temperature_c;
}

void waitUntilTemperatureReached(float target_temperature_c) {
    setTargetTemperature(target_temperature_c);
    mainTaskMachine.setState(&waitForTemperatureState);
}

void startTimer(unsigned long duration_seconds) {
    timer.start(duration_seconds);
    mainTaskMachine.setState(&timerState);
    #ifdef USE_RTC
    auto current = rtc.now().secondstime();
    state.target_timer_time_seconds = current + duration_seconds;
    #endif
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

void skipStep() {
    State* currentState = mainTaskMachine.getCurrentState();
    const char* stateName = currentState->name;

    if (strcmp(stateName, "WaitForTimerStateMachine") == 0) {
        char timer_buffer[8];
        char buffer[30] = "WAIT_TIMER ";
        sprintf(timer_buffer, "%lu", timer.elapsed());
        strcat(buffer, timer_buffer);
        logMessage(buffer);
        return;
    }

    if (strcmp(stateName, "WaitForTemperatureStateMachine") == 0) {
        char temperature_buffer[8];
        char buffer[30] = "WAIT_TIMER ";
        sprintf(temperature_buffer, "%f", state.current_temperature_c);
        strcat(buffer, temperature_buffer);
        logMessage(buffer);
        return;
    }

    mainTaskMachine.setState(&idleState);
}


void startAutoTune() {
    pid_atune.SetOutputStep(100);
    pid_atune.SetLookbackSec(100);
    pid_atune.SetNoiseBand(.05);
    pid_atune.SetControlType(1);  // PI or PID mode
    state.tuning = true;
  }
  