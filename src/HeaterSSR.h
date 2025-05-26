#ifndef HEATER_SSR_H
#define HEATER_SSR_H
#include <Arduino.h>

#include "Components/Heater.h"
#include "Components/Temperature.h"

#include <PID_v1.h>
#include <pidautotuner.h>

typedef void (*on_autotune_ends_callback_t)();
class HeaterSSR : public Heater {
    public:
        HeaterSSR(TemperatureSensor *temperatureSensor, PID *pid, uint8_t pin, double *targetTemperature, double *output, double *input, on_change_callback_t on_change, on_autotune_ends_callback_t on_autotune_ends);
        void setup() override;
        void loop() override;
        void setTargetTemperature(float targetTemperature) override { *this->targetTemperature = targetTemperature; }
        float getTargetTemperature() override { return *this->targetTemperature; }
        void startAutotune(int tuningTemp, int samples) override;
        void stopAutotune() override;

        PID *pid;
    private:

        void softPwm();
        void loopAutotune();

        uint8_t pin;
        TemperatureSensor *temperatureSensor;
        static void monitorTask(void *pvParameters);
        void handleAutotune();

        double * targetTemperature;
        double * output;
        double * input;

        bool isAutotuning;

        PIDAutotuner *tuner = nullptr;
        on_autotune_ends_callback_t on_autotune_ends;

        int8_t pwmFrequency = 10;
    protected:
        xTaskHandle taskHandle;
};

#endif