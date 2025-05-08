#include "Arduino.h"

#include "getTemperature.h"


#ifdef I2C
  Adafruit_ADS1015 ads;  // create an ADS1115 object

#else
  Thermistor* thermistor = NULL;

#endif

float getTemperature() {

  #ifdef I2C
  int16_t adc = ads.readADC_SingleEnded(0); // read from A0
  float volts = ads.computeVolts(adc); // calculate voltage


  float val = 3.3 / volts - 1;
  float resistance = REFERENCE_RESISTANCE / val;

  float steinhart;
  steinhart = resistance / NOMINAL_RESISTANCE;   
  steinhart = log(steinhart);                 
  steinhart /= 3950;                  
  steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15);
  steinhart = 1.0 / steinhart;              
  steinhart -= 273.15;                         

  return steinhart;
  #else


  return thermistor->readCelsius();
#endif
}

void configureTemperatureSensor() {
  #ifdef I2C
  ads.begin();  // initialize the ADS1115

  #else
  #ifdef ESP32
  Thermistor* originThermistor = new NTC_Thermistor_ESP32(
  SENSOR_PIN,
  REFERENCE_RESISTANCE,
  NOMINAL_RESISTANCE,
  NOMINAL_TEMPERATURE,
  B_VALUE,
  3300,
  4095
  );
  #else
    Thermistor* originThermistor = new NTC_Thermistor(
    SENSOR_PIN,
    REFERENCE_RESISTANCE,
    NOMINAL_RESISTANCE,
    NOMINAL_TEMPERATURE,
    B_VALUE,
    4095
    );
  #endif

  thermistor = new AverageThermistor(
  originThermistor,
  READINGS_NUMBER,
  DELAY_TIME
  );
  #endif
}