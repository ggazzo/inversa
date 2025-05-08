#ifndef STATE_H
#define STATE_H
#include "definitions.h"
#include "SD.h"
/**
 * @file state.h
 * 
 * This used in case of power loss
 * We store all the useful information in the sd card
 * later we can use it to restore the state and continue from there
 */


enum StateType {
    WAIT_TEMPERATURE,
    WAIT_TIMER,
    WAIT_CONFIRM,
    PREPARING,
    IDLE
};

#ifndef CURRENT_VERSION
    #define CURRENT_VERSION 1
#endif


#ifndef USE_RTC
    #define USE_RTC
#endif


struct MachineState {
    int version = CURRENT_VERSION;
    StateType current = StateType::IDLE;

    double target_temperature_c;
    double current_temperature_c = 0;
    double output_val;

    bool started = false;
    
    float kp;
    float ki;
    float kd;
    float pOn;
    int time;

    int hysteresis_degrees_c = 1;
    int hysteresis_seconds = 10;
    int volume_liters = 70;
    int power_watts = 3200;


    bool sd_present = false;



    size_t file_position = 0;
    char file_name[30];

    char log_file_name[30];

    char confirm_message[20];
    char message[20] = "";
    char message_line_2[20] = "";
    char message_line_3[20] = "";

    bool tuning = false;

    #if defined(USE_RTC)
    uint32_t target_preparing_time_seconds = 0;
    uint32_t target_timer_time_seconds = 0;
    uint32_t start_total_time_seconds = 0;
    #else

    unsigned long estimated_time_minutes;
    unsigned long timer_current_seconds = 0;
    unsigned long total_timer_count_seconds = 0;

    #endif
};

#endif