#include "Arduino.h"

#include "definitions.h"  

#include "modules.h"


#include "state.h"

extern MachineState state;

#ifdef ESP32
NTC_Thermistor_CustomFormula_ESP32 thermistor( 
  SENSOR_PIN,
  REFERENCE_RESISTANCE,
  NOMINAL_RESISTANCE,
  NOMINAL_TEMPERATURE,
  B_VALUE,
  3300,
  4095
);
#else
NTC_Thermistor thermistor(
    SENSOR_PIN,
    REFERENCE_RESISTANCE,
    NOMINAL_RESISTANCE,
    NOMINAL_TEMPERATURE,
    B_VALUE,
    4095
);

#endif

Thermistor * averageThermistor = new AverageThermistor(
  &thermistor,
  READINGS_NUMBER,
  DELAY_TIME
  );

#include <SimpleKalmanFilter.h>
SimpleKalmanFilter kfilter(2, 2, 0.01);

TemperatureSensor * temperatureSensor = new NTC_Sensor_Temperature(averageThermistor, []() {
  state.current_temperature_c = kfilter.updateEstimate(temperatureSensor->getTemperature());
});

PID *pid = new PID(&state.current_temperature_c, &state.output_val, &state.target_temperature_c,KP, KI,KD, DIRECT);


Heater *heater = new HeaterSSR(temperatureSensor, pid, HEATER_PIN, &state.target_temperature_c, &state.output_val, &state.current_temperature_c, []() {
  // TODO: send output to server 
});