#include "stateMachine.h"

NopState nopState;
IdleStateMachine idleState;
WaitForTemperatureStateMachine waitForTemperatureState;
PreparingStateMachine preparingState;
WaitForTimerStateMachine timerState;
ConfirmStateMachine confirmState;
StateMachine mainTaskMachine([]() {});