#ifndef LOCAL_COMMUNICATION_H
#define LOCAL_COMMUNICATION_H

#include "Communication/CommunicationPeripherals.h"

class LocalCommunication : public CommunicationPeripherals {
    public:
        LocalCommunication();
        LocalCommunication(temp_read_callback_t tempReadCallback, pid_parameters_callback_t pidParametersCallback, volume_callback_t volumeCallback, power_callback_t powerCallback, autotune_callback_t autotuneCallback, stop_autotune_callback_t stopAutotuneCallback);
        ~LocalCommunication();

        void setTargetTemperature(float temperature) override;
        void setPidParameters(float Kp, float Ki, float Kd, float pOn, float sampleTime) override;

        void startAutotune() override;
        void stopAutotune() override;

        void setVolume(float volume) override;
        void setPower(float power) override;
    private:
        temp_read_callback_t tempReadCallback;
        pid_parameters_callback_t pidParametersCallback;
        volume_callback_t volumeCallback;
        power_callback_t powerCallback;
        autotune_callback_t autotuneCallback;
        stop_autotune_callback_t stopAutotuneCallback;
};

#endif
