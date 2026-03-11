#pragma once

#include <Arduino.h>
#include "../core/Plugin.h"
#include "../core/constants.h"
#include "../models/MachineState.h"

class PumpPlugin : public Plugin {
public:
    const char* getName() const override { return "Pump"; }

    bool setup() override {
        pinMode(PIN_PUMP_RELAY, OUTPUT);
        digitalWrite(PIN_PUMP_RELAY, LOW);
        gState.pumpOn = false;

        // Listen for pump state changes
        bus().subscribe(EventType::PumpStateChanged, [this](const Event& e) {
            if (e.boolValue) {
                turnOn();
            } else {
                turnOff();
            }
        });

        DEBUG_PRINTF("[Pump] Relay on GPIO %d\n", PIN_PUMP_RELAY);
        return true;
    }

    void loop() override {
        // Pump is simple on/off — no periodic work needed
    }

    void turnOn() {
        digitalWrite(PIN_PUMP_RELAY, HIGH);
        gState.pumpOn = true;
        DEBUG_PRINTLN("[Pump] ON");
    }

    void turnOff() {
        digitalWrite(PIN_PUMP_RELAY, LOW);
        gState.pumpOn = false;
        DEBUG_PRINTLN("[Pump] OFF");
    }

    bool isOn() const { return gState.pumpOn; }
};
