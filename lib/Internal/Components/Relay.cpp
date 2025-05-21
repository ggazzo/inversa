#include "Relay.h"

Relay::Relay(int pin, uint8_t state, on_change_callback_t on_change) : state(false) {
    this->pin = pin;
    this->state = state;
}

void Relay::setup() {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, state);
}

void Relay::set_state(bool state) {
    this->state = state;
    digitalWrite(pin, state ? state : !state);
}

void Relay::loop() {

}