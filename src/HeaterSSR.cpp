#include "HeaterSSR.h"

#define LOW_TEMP_BAND 15 // degrees
#define HIGH_TEMP_BAND 1.0f // degrees
#define AUTOTUNE_INPUT_SPAN 100.0f // degrees
#define AUTOTUNE_OUTPUT_SPAN 1000 // degrees
#define AUTOTUNE_OUTPUT_START 0.0f // degrees
#define AUTOTUNE_OUTPUT_STEP 1.0f // degrees
#define AUTOTUNE_TIME_STEP 1.0f // interval = testTimeSec / samples
#define AUTOTUNE_SETTLE_TIME_SECONDS 120.0f // seconds
#define AUTOTUNE_SAMPLES 500 // samples

HeaterSSR::HeaterSSR(TemperatureSensor *temperatureSensor, PID *pid, uint8_t pin, double *targetTemperature, double *output, double *input, on_change_callback_t on_change) {
    this->temperatureSensor = temperatureSensor;
    this->pid = pid;
    this->pin = pin;
    this->targetTemperature = targetTemperature;
    this->output = output;
    this->input = input;
    this->on_change = on_change;

    this->isAutotuning = false;
}

void HeaterSSR::setup() {
    xTaskCreate(monitorTask, "HeaterSSR::monitor", configMINIMAL_STACK_SIZE * 4, this, 1, &taskHandle);
    pinMode(pin, OUTPUT);
}

void HeaterSSR::startAutotune() {
    if (!isAutotuning) {
        isAutotuning = true;
        tuner.Configure(AUTOTUNE_INPUT_SPAN, AUTOTUNE_OUTPUT_SPAN, AUTOTUNE_OUTPUT_START, AUTOTUNE_OUTPUT_STEP, AUTOTUNE_TIME_STEP, AUTOTUNE_SETTLE_TIME_SECONDS, AUTOTUNE_SAMPLES);
        pid->SetMode(MANUAL);
    }
}

void HeaterSSR::stopAutotune() {
    if (isAutotuning) {
        isAutotuning = false;
        pid->SetMode(AUTOMATIC);
    }
}

void HeaterSSR::loop() {
    this->tuner.softPwm(pin, *input, *output, *targetTemperature, AUTOTUNE_OUTPUT_SPAN, 1);

    switch (this->tuner.Run()) {
        case tuner.sample:
            tuner.plotter(*input, *output, *targetTemperature,  0.1f, 3);
            break;
        case tuner.tunings:
            this->tuner.GetAutoTunings(&kp, &ki, &kd); // sketch variables updated by sTune
            pid->SetOutputLimits(0, 255);
            pid->SetSampleTime(255 - 1);
            pid->SetMode(AUTOMATIC); // the PID is turned on
            pid->SetTunings(kp, ki, kd); // update PID with the new tunings
            break;
        case tuner.runPid:
            this->isAutotuning = false;
            pid->Compute();
            break;
    }

    // if (this->input - this->targetTemperature > LOW_TEMP_BAND) {
    //     pid->SetMode(MANUAL);
    //     *output = 255;
    //     digitalWrite(pin, HIGH);
    //     return;
    // }

    // // second case, the temperature is too high
    // if (this->input > this->targetTemperature + HIGH_TEMP_BAND) {
    //     pid->SetMode(MANUAL);
    //     *output = 0;
    //     digitalWrite(pin, LOW);
    //     return;
    // }

    // pid->SetMode(AUTOMATIC);
    // pid->Compute();

    // uint32_t now = millis();

    // static uint32_t windowStartTime, nextSwitchTime;

    // if ((now - windowStartTime) > 1000) {
    //     windowStartTime += 1000;
    // }

    // if ((now - windowStartTime) < 1000 * (*output / 255.0)) {
    //     digitalWrite(pin, HIGH);
    // } else {
    //     digitalWrite(pin, LOW);
    // }

    this->on_change();
}

[[noreturn]] void HeaterSSR::monitorTask(void *pvParameters) {
    HeaterSSR *self = (HeaterSSR *)pvParameters;
    while (true) {
        self->loop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}