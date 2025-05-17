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
    #define PWM_CHANNEL 0       // LEDC channel (0-15)
    #define PWM_RESOLUTION 7    // PWM resolution in bits (e.g., 8 bits = 0-255 duty cycle)
    #define PWM_FREQUENCY 120   // PWM frequency in Hz
    // ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);  // Configure channel
    // ledcAttachPin(pin, PWM_CHANNEL);  // Attach the channel to the GPIO to be controlled
    pinMode(pin, OUTPUT);
}

void HeaterSSR::loop() {
    if (this->input - this->targetTemperature > LOW_TEMP_BAND) {
        pid->SetMode(MANUAL);
        *output = 255;
        return;
    }

    // second case, the temperature is too high
    if (this->input > this->targetTemperature + HIGH_TEMP_BAND) {
        pid->SetMode(MANUAL);
        *output = 0;
        return;
    }


    pid->SetMode(AUTOMATIC);
    pid->Compute();


    // lets help our PID to reach the target temperature
    // if we know the volume, the power and the target temperature
    // we can at least guess the minimum in watts to keep the temperature
    // at the target temperature but to avoid any overheating lets slightly reduce the watts

    // double watts = calculateMinimumPower(settings.getVolumeLiters()/ 1000, calculateSurfaceAreaFromVolume(settings.getVolumeLiters()/ 1000, .30) , state.target_temperature_c, 25);

    // int to_PWM = map(watts * .7, 0, settings.getPowerWatts(), 0, 255);
    // machineState.output_val = constrain(MAX(machineState.output_val, to_PWM), 0, 255);

    analogWrite(pin, *output);
    this->on_change();
}

[[noreturn]] void HeaterSSR::monitorTask(void *pvParameters) {
    HeaterSSR *self = (HeaterSSR *)pvParameters;
    while (true) {
        self->loop();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}