#include "HeaterSSR.h"

#define LOW_TEMP_BAND 15 // degrees
#define HIGH_TEMP_BAND 1 // degrees


HeaterSSR::HeaterSSR(TemperatureSensor *temperatureSensor, PID *pid, uint8_t pin, double *targetTemperature, double *output, double *input, on_change_callback_t on_change) {
    this->temperatureSensor = temperatureSensor;
    this->pid = pid;
    this->pin = pin;
    this->targetTemperature = targetTemperature;
    this->output = output;
    this->input = input;
    this->on_change = on_change;
}

void HeaterSSR::setup() {
    xTaskCreate(monitorTask, "HeaterSSR::monitor", configMINIMAL_STACK_SIZE * 4, this, 1, &taskHandle);
    pinMode(pin, OUTPUT);
}

void HeaterSSR::loop() {
    if (this->input - this->targetTemperature > LOW_TEMP_BAND) {
        pid->SetMode(MANUAL);
        *output = 255;
        digitalWrite(pin, HIGH);
        return;
    }

    // second case, the temperature is too high
    if (this->input > this->targetTemperature + HIGH_TEMP_BAND) {
        pid->SetMode(MANUAL);
        *output = 0;
        digitalWrite(pin, LOW);
        return;
    }

    pid->SetMode(AUTOMATIC);
    pid->Compute();

    uint32_t now = millis();

    static uint32_t windowStartTime, nextSwitchTime;

    if ((now - windowStartTime) > 1000) {
        windowStartTime += 1000;
    }

    if ((now - windowStartTime) < 1000 * (*output / 255.0)) {
        digitalWrite(pin, HIGH);
    } else {
        digitalWrite(pin, LOW);
    }

    this->on_change();
}

[[noreturn]] void HeaterSSR::monitorTask(void *pvParameters) {
    HeaterSSR *self = (HeaterSSR *)pvParameters;
    while (true) {
        self->loop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}