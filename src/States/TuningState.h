#ifndef TUNINGSTATE_H
#define TUNINGSTATE_H

#include <Arduino.h>
#include <StateMachine.h>
#include <CountDown.h>
#include "media.h"
#include "calc.h"

#include "modules.h"


class TuningStateMachine : public State {

   public:
      virtual ~TuningStateMachine() = default;
      TuningStateMachine() : State("TuningStateMachine") {}
      void enter() override {
         controller->setState(StateType::TUNING);

      }
      void run() override {
      }

      void exit() override {

      }
};

#endif