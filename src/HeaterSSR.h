#ifndef HEATER_SSR_H
#define HEATER_SSR_H
#include <Arduino.h>

#include "Components/Heater.h"
#include "Components/Temperature.h"

#include <PID_v1.h>
#include <sTune.h>

class HeaterSSR : public Heater {
    public:
        HeaterSSR(TemperatureSensor *temperatureSensor, PID *pid, uint8_t pin, double *targetTemperature, double *output, double *input, on_change_callback_t on_change);
        void setup() override;
        void loop() override;
        void setTargetTemperature(float targetTemperature) override { *this->targetTemperature = targetTemperature; }
        float getTargetTemperature() override { return *this->targetTemperature; }
        void startAutotune() override;
        void stopAutotune() override;
        PID *pid;
    private:
        uint8_t pin;
        TemperatureSensor *temperatureSensor;
        static void monitorTask(void *pvParameters);
        void handleAutotune();

        double * targetTemperature;
        double * output;
        double * input;
        bool isAutotuning;
        sTune tuner;

        float kp, ki, kd = 50;

    protected:
        xTaskHandle taskHandle;
};

#endif