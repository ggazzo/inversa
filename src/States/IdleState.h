#pragma once
#include <StateMachine.h>
#include "command.h"
#include "media.h"
#include "state.h"

extern StateMachine mainTaskMachine;
extern MachineState state;

class IdleStateMachine : public State {

   public:
      IdleStateMachine() : State("IdleStateMachine") {}


      void enter() override {
        controller->setState(StateType::IDLE);
        handlePowerLoss();
      }
      void run() override {
          readCommands();
      }
};
