#ifndef LOCAL_COMMUNICATION_H
#define LOCAL_COMMUNICATION_H

#include "Communication/CommunicationPeripherals.h"

class LocalCommunication : public CommunicationPeripherals {
    public:
        LocalCommunication();
        LocalCommunication(temp_read_callback_t tempReadCallback, pid_parameters_callback_t pidParametersCallback, volume_callback_t volumeCallback, power_callback_t powerCallback);
        ~LocalCommunication();

        void setTargetTemperature(float temperature) override;
        void setPidParameters(float Kp, float Ki, float Kd, float pOn, float sampleTime) override;
        void setVolume(float volume) override;
        void setPower(float power) override;

        void setTempReadCallback(temp_read_callback_t callback) override;
        void setPidParametersCallback(pid_parameters_callback_t callback) override;
        void setVolumeCallback(volume_callback_t callback) override;
        void setPowerCallback(power_callback_t callback) override;
    private:
        temp_read_callback_t tempReadCallback;
        pid_parameters_callback_t pidParametersCallback;
        volume_callback_t volumeCallback;
        power_callback_t powerCallback;
};

#endif
