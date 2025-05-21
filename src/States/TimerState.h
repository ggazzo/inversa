#pragma once

#include <Arduino.h>
#include <CountDown.h>

#include "timer.h"
#include "media.h"


extern IdleStateMachine idleState;

class WaitForTimerStateMachine : public State {

   public:
      WaitForTimerStateMachine() : State("WaitForTimerStateMachine") {}
      virtual ~WaitForTimerStateMachine() = default;
      void enter() override {
        controller->setState(StateType::WAIT_TIMER);
        handlePowerLoss();
      }


      void run() override {
          if(timer.isFinished()) {
              controller->skip();
          }
      }

      void exit() override {
        timer.stop();
      }
};
