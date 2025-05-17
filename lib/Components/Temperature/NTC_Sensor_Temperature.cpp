#include "NTC_Sensor_Temperature.h"

NTC_Sensor_Temperature::NTC_Sensor_Temperature(Thermistor *thermistor, on_change_callback_t on_change) {
    this->thermistor = thermistor;
    this->on_change = on_change;
}

float NTC_Sensor_Temperature::getTemperature() {
    return thermistor->readCelsius();
}


void NTC_Sensor_Temperature::setup() {
    xTaskCreate(monitorTask, "NTC_Sensor_Temperature::monitor", configMINIMAL_STACK_SIZE * 4, this, 1, &taskHandle);
}


// runs 10 times per second
[[noreturn]] void NTC_Sensor_Temperature::monitorTask(void *pvParameters) {
    NTC_Sensor_Temperature *self = (NTC_Sensor_Temperature *)pvParameters;
    while (true) {
        self->loop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void NTC_Sensor_Temperature::loop() {
    this->getTemperature();
    this->on_change();
}

NTC_Sensor_Temperature::~NTC_Sensor_Temperature() {
    if (this->taskHandle != NULL) {
        vTaskDelete(this->taskHandle);
        this->taskHandle = NULL;
    }
}