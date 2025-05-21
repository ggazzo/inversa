#ifndef PREPARINGSTATE_H
#define PREPARINGSTATE_H

#include <Arduino.h>
#include <StateMachine.h>
#include <CountDown.h>
#include "media.h"
#include "calc.h"

#include "modules.h"

/**
 * Heating process can take a long time, so maybe you want to prepare your equipment
 * to be at x time at y temperature
 * Based on the formula and considering the power/volume ratio, you can estimate the time required to heat
 * 
 * if the estimated required time is smaller than the remaining time/target configured, we can wait
 * if there ir no extra time to wait, we can go to the next state
 */

class PreparingStateMachine : public State {

   public:
      virtual ~PreparingStateMachine() = default;
      PreparingStateMachine() : State("PreparingStateMachine") {}
      CountDown countDown = CountDown(CountDown::SECONDS);
      float time_seconds = 0;

      float volume_liters = 0;
      float power_watts = 0;
      float target_temperature_c = 0;

      void enter() override {

         if (time_seconds){
            countDown.start(time_seconds);
         }

         controller->setState(StateType::PREPARING);

         handlePowerLoss();
      }
      void run() override {
         if(countDown.isStopped())
         {
            controller->skip();
            return;
         }

         if(countDown.remaining() <= calculateHeatingTime_seconds(volume_liters, power_watts, controller->getTemperature(), target_temperature_c))
         {
            controller->setTargetTemperature(target_temperature_c);
         } else {
            controller->setTargetTemperature(0);
         }
      }

      void exit() override {
         countDown.stop();
      }
};

#endif