#include "Arduino.h"

#include "Components/Temperature.h"
#include "Components/Heater.h"
#include "Components/Relay.h"
#include "Controller/PeripheralController.h"
#include <PID_v1.h>
#include "Components/Temperature/NTC_Sensor_Temperature.h"
#include "HeaterSSR.h"

#include "Communication/CommunicationPeripherals.h"


#include "definitions.h"  
#include "modules.h"
#include "state.h"
#include "States/stateMachine.h"
#include "LocalCommunication.h"
#include "LocalController/LocalController.h"

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

#include <SimpleKalmanFilter.h>
SimpleKalmanFilter kfilter(1, 1, 0.01);

TemperatureSensor * temperatureSensor = new NTC_Sensor_Temperature(&thermistor, []() {
  state.current_temperature_c = kfilter.updateEstimate(temperatureSensor->getTemperature());
});

PID *pid = new PID(&state.current_temperature_c, &state.output_val, &state.target_temperature_c,KP, KI,KD, DIRECT);

Heater *heater = new HeaterSSR(temperatureSensor, pid, HEATER_PIN, &state.target_temperature_c, &state.output_val, &state.current_temperature_c, []() {

  

}, []() {
  settings.setKp(
    ((HeaterSSR*)heater)->pid->GetKp()
  );
  settings.setKi(
    ((HeaterSSR*)heater)->pid->GetKi()
  );
  settings.setKd(
    ((HeaterSSR*)heater)->pid->GetKd()
  );


  mainTaskMachine.setState(&idleState);

});

Relay *pump = new Relay(PUMP_PIN, 0, []() {
  // TODO: send output to server 
});

Settings settings;

PeripheralController *peripheralController = new PeripheralController(temperatureSensor, heater, pump);

// Initialize state machine with idle state12345678

StateMachine mainTaskMachine(&idleState, []() {});

// mainTaskMachine.init(&idleState);

CommunicationPeripherals *communicationPeripherals = new LocalCommunication(
  [](float targetTemperature) {
    state.target_temperature_c = targetTemperature;
    heater->setTargetTemperature(targetTemperature);
  },
  [](float kp, float ki, float kd, float pOn, float sampleTime) {
    settings.setKp(kp);
    settings.setKi(ki);
    settings.setKd(kd);
    settings.setPOn(pOn);
    settings.setTime(sampleTime);
    pid->SetTunings(kp, ki, kd, pOn);
    pid->SetSampleTime(sampleTime);
    pid->SetMode(AUTOMATIC);
  },
  [](float volume) {
    settings.setVolumeLiters(volume);
  },
  [](float power) {
    settings.setPowerWatts(power);
  },
  [](float targetTemperature, int samples) {
    Serial.println("Starting autotune");
    mainTaskMachine.setState(&tuningState);
    heater->startAutotune(targetTemperature, samples);
  },
  []() {
    Serial.println("Stopping autotune");
    heater->stopAutotune();
  }
);

RTC_DS1307 *rtc = new RTC_DS1307(); 

MainController<StateType, Steps> *controller = new LocalController(&mainTaskMachine, &settings, communicationPeripherals, rtc, &state, peripheralController);