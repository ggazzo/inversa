#include "LocalCommunication.h"

LocalCommunication::LocalCommunication() {
}

LocalCommunication::LocalCommunication(temp_read_callback_t tempReadCallback, pid_parameters_callback_t pidParametersCallback, volume_callback_t volumeCallback, power_callback_t powerCallback, autotune_callback_t autotuneCallback, stop_autotune_callback_t stopAutotuneCallback) {
    this->tempReadCallback = tempReadCallback;
    this->pidParametersCallback = pidParametersCallback;
    this->volumeCallback = volumeCallback;
    this->powerCallback = powerCallback;
}

LocalCommunication::~LocalCommunication() {
}

void LocalCommunication::setTargetTemperature(float temperature) {
    if (tempReadCallback) {
        tempReadCallback(temperature);
    }
}

void LocalCommunication::setPidParameters(float Kp, float Ki, float Kd, float pOn, float sampleTime) {
    if (pidParametersCallback) {
        pidParametersCallback(Kp, Ki, Kd, pOn, sampleTime);
    }
}   

void LocalCommunication::setVolume(float volume) {
    if (volumeCallback) {
        volumeCallback(volume);
    }
}

void LocalCommunication::setPower(float power) {
    if (powerCallback) {
        powerCallback(power);
    }
}

void LocalCommunication::startAutotune() {
    if (autotuneCallback) {
        autotuneCallback();
    }
}

void LocalCommunication::stopAutotune() {
    if (stopAutotuneCallback) {
        stopAutotuneCallback();
    }
}