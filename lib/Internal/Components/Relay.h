#ifndef RELAY_H
#define RELAY_H
#include <Arduino.h>
#include "Base.h"

class Relay : public Base {
    public:
        Relay(int pin, uint8_t state, on_change_callback_t on_change);
        ~Relay();

        void setup() override;
        void loop() override;

        void set_state(bool state);

    private:
        int pin;
        bool state;
};

#endif
