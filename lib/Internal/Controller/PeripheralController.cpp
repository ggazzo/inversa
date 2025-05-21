#include "PeripheralController.h"

PeripheralController::PeripheralController(TemperatureSensor *temperature, Heater *heater, Relay *pump) {
    this->temperature = temperature;
    this->heater = heater;
    this->pump = pump;
}

PeripheralController::~PeripheralController() {
}

void PeripheralController::setup() {
    temperature->setup();
    heater->setup();
    pump->setup();
}

void PeripheralController::loop() {
    temperature->loop();
    heater->loop();
    pump->loop();
}