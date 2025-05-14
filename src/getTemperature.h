#include "definitions.h"


#ifdef I2C
#include <Adafruit_ADS1X15.h>


#else

#include <Thermistor.h>
#include <NTC_Thermistor_CustomFormula_ESP32.h>
#include <AverageThermistor.h>
/**
  How many readings are taken to determine a mean temperature.
  The more values, the longer a calibration is performed,
  but the readings will be more accurate.
*/
#define READINGS_NUMBER 10

/**
  Delay time between a temperature readings
  from the temperature sensor (ms).
*/
#define DELAY_TIME 10


#endif

float getTemperature();

void configureTemperatureSensor();