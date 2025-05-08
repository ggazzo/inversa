#pragma once
#include <StateMachine.h>
#include "command.h"

extern StateMachine mainTaskMachine;


class ConfirmStateMachine : public State {

   public:
      ConfirmStateMachine() : State("ConfirmState") {}
      virtual ~ConfirmStateMachine() = default;

      void enter() override {
        state.current = StateType::WAIT_CONFIRM;
      }
      void run() override {
          readCommands();
      }

      void exit() override {
      }

};
