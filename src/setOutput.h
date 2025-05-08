
#include "Arduino.h"
#include "definitions.h"

#ifdef I2C
/* Libs */
#include "PCA9685.h"
/* Defines */
#define ADDRESS     0x40
#define FREQUENCY   24     //min 24Hz, max 1524Hz
#define HEATER_PIN 0
#endif

void configureOutputs();
void setOutput(int outputVal);