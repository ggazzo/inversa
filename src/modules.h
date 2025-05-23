#include "definitions.h"

#include <PID_v1.h>

#include <NTC_Thermistor_CustomFormula_ESP32.h>
#include <Thermistor.h>
#include <AverageThermistor.h>

#include "Components/Temperature.h"
#include "Components/Heater.h"
#include "Components/Relay.h"

#include "Controller/PeripheralController.h"
#include "Controller/MainController.h"

#include "state.h"

/**
  How many readings are taken to determine a mean temperature.
  The more values, the longer a calibration is performed,
  but the readings will be more accurate.
*/
#define READINGS_NUMBER 100

/**
  Delay time between a temperature readings
  from the temperature sensor (ms).
*/
#define DELAY_TIME 1

// Declare temperatureSensor as extern so it can be used in other files
extern PID *pid;


extern Settings settings;
extern MainController<StateType> *controller;
extern CommunicationPeripherals *communicationPeripherals;