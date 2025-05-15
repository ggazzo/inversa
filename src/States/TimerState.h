#pragma once

#include <Arduino.h>
#include <CountDown.h>

#include "IdleState.h"

#include "timer.h"
#include "IdleState.h"
#include "media.h"


extern IdleStateMachine idleState;

class WaitForTimerStateMachine : public State {

   public:
      WaitForTimerStateMachine() : State("WaitForTimerStateMachine") {}
      virtual ~WaitForTimerStateMachine() = default;
      void enter() override {
        state.current = StateType::WAIT_TIMER;
        handlePowerLoss();
      }


      void run() override {
          if(timer.isFinished()) {
              Serial.println("Timer finished");
              mainTaskMachine.setState(&idleState);
          }
          // menuTimer.setCurrentValue(timer.timeRemaining()/60);
      }

      void exit() override {
        timer.stop();
      }
};
