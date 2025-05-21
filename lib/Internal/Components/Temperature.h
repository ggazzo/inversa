#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include "Base.h"

 class TemperatureSensor : public Base {
    public:
        virtual float getTemperature() = 0;
        virtual void setup() override = 0;
        virtual void loop() override = 0;
};

#endif
