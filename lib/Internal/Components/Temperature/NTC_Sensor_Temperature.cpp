#include "NTC_Sensor_Temperature.h"

constexpr int AVERAGE_SAMPLES = 50;

NTC_Sensor_Temperature::NTC_Sensor_Temperature(Thermistor *thermistor, on_change_callback_t on_change) {
    this->thermistor = thermistor;
    this->on_change = on_change;
}

float NTC_Sensor_Temperature::getTemperature() {
    return this->temperature;
}


void NTC_Sensor_Temperature::setup() {
    xTaskCreate(monitorTask, "NTC_Sensor_Temperature::monitor", configMINIMAL_STACK_SIZE * 4, this, 1, &taskHandle);
}


// runs 10 times per second
[[noreturn]] void NTC_Sensor_Temperature::monitorTask(void *pvParameters) {
    NTC_Sensor_Temperature *self = (NTC_Sensor_Temperature *)pvParameters;
    while (true) {
        float temperature = self->thermistor->readCelsius();
        for (int i = 0; i < AVERAGE_SAMPLES; i++) {
            temperature += round(self->thermistor->readCelsius() * 100) / 100.0;
            vTaskDelay(pdMS_TO_TICKS(10));
        }

        self->temperature = round(temperature *100 / AVERAGE_SAMPLES  ) / 100.0;
        self->on_change();
    }
}

void NTC_Sensor_Temperature::loop() {
    
}

NTC_Sensor_Temperature::~NTC_Sensor_Temperature() {
    if (this->taskHandle != NULL) {
        vTaskDelete(this->taskHandle);
        this->taskHandle = NULL;
    }
}