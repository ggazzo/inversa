#ifndef COMMUNICATION_PERIPHERALS_H
#define COMMUNICATION_PERIPHERALS_H

#include <functional>
using temp_read_callback_t = std::function<void(float temperature)>;
using pid_parameters_callback_t = std::function<void(float Kp, float Ki, float Kd, float pOn, float sampleTime)>;
using volume_callback_t = std::function<void(float volume)>;
using power_callback_t = std::function<void(float power)>;

class CommunicationPeripherals {
    public:
        // CommunicationPeripherals();
        // ~CommunicationPeripherals();
        virtual void setTargetTemperature(float temperature) = 0;
        virtual void setPidParameters(float Kp, float Ki, float Kd, float pOn, float sampleTime) = 0;
        virtual void setVolume(float volume) = 0;
        virtual void setPower(float power) = 0;

        virtual void setTempReadCallback(temp_read_callback_t callback) = 0;
        virtual void setPidParametersCallback(pid_parameters_callback_t callback) = 0;
        virtual void setVolumeCallback(volume_callback_t callback) = 0;
        virtual void setPowerCallback(power_callback_t callback) = 0;
    private:
};


#endif
