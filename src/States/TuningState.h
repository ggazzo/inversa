#ifndef TUNINGSTATE_H
#define TUNINGSTATE_H

#include <Arduino.h>
#include <StateMachine.h>
#include "modules.h"


class TuningStateMachine : public State {

   public:
      virtual ~TuningStateMachine() = default;
      TuningStateMachine() : State("TuningStateMachine") {}
      void enter() override {
         controller->setState(StateType::TUNING);

      }
      void run() override {
        readCommands();
      }

      void exit() override {

      }
};

#endif