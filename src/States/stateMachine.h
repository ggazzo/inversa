#ifndef INVERSA_STATEMACHINE_H
#define INVERSA_STATEMACHINE_H

#include <Arduino.h>
#include <StateMachine.h>
#include "PreparingState.h"
#include "IdleState.h"
#include "TimerState.h"
#include "ConfirmState.h"


class NopState : public State {

    public:
        NopState() : State("NopState") {}
        virtual ~NopState() = default;

        void enter() override {
        }
};

extern IdleStateMachine idleState;
extern NopState nopState;
extern WaitForTemperatureStateMachine waitForTemperatureState;
extern PreparingStateMachine preparingState;
extern WaitForTimerStateMachine timerState;
extern ConfirmStateMachine confirmState;
extern StateMachine mainTaskMachine;

#endif