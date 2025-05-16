#ifndef WAIT_FOR_TEMPERATURE_STATE_H
#define WAIT_FOR_TEMPERATURE_STATE_H

#include <Arduino.h>
#include <StateMachine.h>
#include <CountDown.h>
#include "States/IdleState.h"
#include "calc.h"
#include "state.h"
#include "media.h"
#include "Settings.h"

extern MachineState state;

extern IdleStateMachine idleState;

extern StateMachine mainTaskMachine;

extern MachineState state;

extern Settings settings;

/**
 * Heating process can take a long time, so maybe you want to prepare your equipment
 * to be at x time at y temperature
 * Based on the formula and considering the power/volume ratio, you can estimate the time required to heat
 *
 * if the estimated required time is smaller than the remaining time/target configured, we can wait
 * if there ir no extra time to wait, we can go to the next state
 */

class WaitForTemperatureStateMachine : public State {

   public:
    WaitForTemperatureStateMachine() : State("WaitForTemperatureStateMachine") {}
    virtual ~WaitForTemperatureStateMachine() = default;
      bool is_in_hysteresis = false;
      float elapsed_time_after_hysteresis_seconds = 0;
      
      double * current_temperature_c;

   void enter() override {
      Serial.println("Entering WaitForTemperatureStateMachine");
     current_temperature_c = &state.current_temperature_c;
     state.current = StateType::WAIT_TEMPERATURE;
     handlePowerLoss();
   }

   void run() override {
      float current_time_seconds = MILLIS_TO_SECONDS(millis());
      float temperature_diff_c = abs(*current_temperature_c - state.target_temperature_c);

      if (is_in_hysteresis == false)
     {
       if (temperature_diff_c <= settings.getHysteresisDegreesC())
       {
         is_in_hysteresis = true;
         elapsed_time_after_hysteresis_seconds = current_time_seconds;
       }
       return;
     }

    if (temperature_diff_c > settings.getHysteresisDegreesC())
    {
       elapsed_time_after_hysteresis_seconds = current_time_seconds;
       return;
    }

     if (current_time_seconds > elapsed_time_after_hysteresis_seconds + settings.getHysteresisSeconds())
     {
       mainTaskMachine.setState(&idleState);
       return;
     }
   }

   void exit() override {
    is_in_hysteresis = false;
   }
};


#endif