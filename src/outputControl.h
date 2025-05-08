
#ifndef OUTPUT_CONTROL_H
#define OUTPUT_CONTROL_H
#include "Arduino.h"
#include "state.h"
#include "setOutput.h"
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>


void outputControl(MachineState &machineState);
#endif