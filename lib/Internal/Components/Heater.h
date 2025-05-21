#ifndef HEATER_H
#define HEATER_H

#include "Temperature.h"

class Heater : public Base {
    public:

        virtual void setTargetTemperature(float targetTemperature) = 0;
        virtual float getTargetTemperature() = 0;

        virtual void startAutotune() = 0;
        virtual void stopAutotune() = 0;

};

#endif
