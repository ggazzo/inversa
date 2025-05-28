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

enum Steps
{
    NONE,
    PRE_HEATING,
    MASHING,
    MASH_OUT,
    SPARGE,
    BOILING,
    COOLING,
    DONE,
};

enum StateType {
    WAIT_TEMPERATURE,
    WAIT_TIMER,
    WAIT_CONFIRM,
    PREPARING,
    IDLE,
    TUNING,
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
    Steps step = Steps::NONE;

    double target_temperature_c = 0;
    double current_temperature_c = 0;
    double output_val = 0;

    bool started = false;
    bool tuning = false;
    bool sd_present = false;

    size_t file_position = 0;
    char file_name[30];

    uint32_t total_time_seconds_start = 0;
    uint32_t total_time_seconds_estimated = 0;

    uint32_t step_time_seconds_start = 0;
    uint32_t step_time_seconds_estimated = 0;

    uint32_t target_timer_time_seconds = 0;

    char message[20] = "";



    // #if defined(USE_RTC)
    // uint32_t target_preparing_time_seconds = 0;
    // uint32_t target_timer_time_seconds = 0;
    // uint32_t start_total_time_seconds = 0;
    // #else

    // unsigned long estimated_time_minutes;
    // unsigned long timer_current_seconds = 0;
    // unsigned long total_timer_count_seconds = 0;

    // #endif
};

#endif