#ifndef PERIPHERAL_CONTROLLER_H
#define PERIPHERAL_CONTROLLER_H

#include "Controller.h"

#include "Components/Temperature.h"
#include "Components/Heater.h"
#include "Components/Relay.h"


class PeripheralController: public Controller {
    public:
        PeripheralController(TemperatureSensor *temperature, Heater *heater, Relay *pump);
        ~PeripheralController();


        void setup() override;
        void loop() override;

    private:
        TemperatureSensor *temperature;
        Heater *heater;
        Relay *pump;
};


#endif
