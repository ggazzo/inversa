#include "HeaterSSR.h"

constexpr float TUNER_INPUT_SPAN = 160.0f;
constexpr float TUNER_OUTPUT_SPAN = 255;

const char *TAG = "HeaterSSR";

HeaterSSR::HeaterSSR(TemperatureSensor *temperatureSensor, PID *pid, uint8_t pin, double *targetTemperature, double *output, double *input, on_change_callback_t on_change, on_autotune_ends_callback_t on_autotune_ends) {
    this->temperatureSensor = temperatureSensor;
    this->pid = pid;
    this->pin = pin;
    this->targetTemperature = targetTemperature;
    this->output = output;
    this->input = input;
    this->on_change = on_change;
    this->on_autotune_ends = on_autotune_ends;
    this->isAutotuning = false;

    this->tuner = new PIDAutotuner();
}

void HeaterSSR::setup() {
    xTaskCreate(monitorTask, "HeaterSSR::monitor", configMINIMAL_STACK_SIZE * 4, this, 1, &taskHandle);
    pinMode(pin, OUTPUT);
}

void HeaterSSR::startAutotune(int tuningTemp, int samples) {
    if (!isAutotuning) {
        isAutotuning = true;
        *targetTemperature = tuningTemp;
        tuner->setOutputRange(0, TUNER_OUTPUT_SPAN);
        tuner->setTargetInputValue(tuningTemp);
        tuner->setTuningCycles(samples);
        tuner->setLoopInterval((TUNER_OUTPUT_SPAN - 1) * 1000);
        tuner->setZNMode(PIDAutotuner::ZNModeLessOvershoot);
        pid->SetMode(MANUAL);
    }
}

void HeaterSSR::stopAutotune() {
    if (isAutotuning) {
        isAutotuning = false;
        pid->SetMode(AUTOMATIC);
        *targetTemperature = 0;
        ESP_LOGI(TAG, "stopAutotune");
        this->on_autotune_ends();
    }
}

void HeaterSSR::loopAutotune() {
    static unsigned long lastMicros = 0;
    static bool tuningStarted = false;
    static long loopInterval = (static_cast<long>(TUNER_OUTPUT_SPAN) - 1L) * 1000L;

    if (!tuningStarted) {
        tuner->startTuningLoop(micros());
        tuningStarted = true;
        lastMicros = micros();
        return;
    }

    if (tuner->isFinished()) {
        *this->output = 0;
        softPwm();
        this->pid->SetTunings(tuner->getKp(), tuner->getKi(), tuner->getKd());
        ESP_LOGI("sTune", "Kp: %.2f, Ki: %.2f, Kd: %.2f", tuner->getKp(), tuner->getKi(), tuner->getKd());
        this->stopAutotune();
        isAutotuning = false;
        tuningStarted = false;
        return;
    }

    unsigned long currentMicros = micros();
    if (currentMicros - lastMicros >= loopInterval) {
        *this->output = tuner->tunePID(*this->input, currentMicros);
        softPwm();
        ESP_LOGI("sTune", "Setpoint: %.2f, Input: %.2f, Output: %.2f", *this->targetTemperature, *this->input, *this->output * 1.0);
        lastMicros = currentMicros;
    }

    softPwm();
}

void HeaterSSR::softPwm() {
    static unsigned long pwmStart = 0;
    unsigned long now = millis();
    unsigned long elapsed = now - pwmStart;

    uint32_t pwm_period_ms = 1000 / this->pwmFrequency;

    if (elapsed >= pwm_period_ms) {
        pwmStart = now;
        elapsed = 0;
    }

    // Compute ON time in ms based directly on PID output scale
    uint32_t pwmDutyMs = (*this->output * pwm_period_ms) / TUNER_OUTPUT_SPAN;

    digitalWrite(this->pin, elapsed < pwmDutyMs);
}



void HeaterSSR::loop() {
    if (isAutotuning) {
        this->loopAutotune();
        return;
    }
    softPwm();
    if (pid->Compute()) {
        ESP_LOGI("HeaterSSR", "Output: %.2f", *this->output);
    }
}

[[noreturn]] void HeaterSSR::monitorTask(void *pvParameters) {
    HeaterSSR *self = (HeaterSSR *)pvParameters;
    while (true) {
        self->loop();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}