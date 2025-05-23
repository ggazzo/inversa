#ifndef INVERSA_STATEMACHINE_H
#define INVERSA_STATEMACHINE_H

#include <Arduino.h>
#include <StateMachine.h>
#include "PreparingState.h"
#include "IdleState.h"
#include "TimerState.h"
#include "ConfirmState.h"
#include "WaitForTemperatureState.h"
#include "TuningState.h"


extern IdleStateMachine idleState;
extern WaitForTemperatureStateMachine waitForTemperatureState;
extern PreparingStateMachine preparingState;
extern WaitForTimerStateMachine timerState;
extern ConfirmStateMachine confirmState;
extern TuningStateMachine tuningState;
extern StateMachine mainTaskMachine;

#endif