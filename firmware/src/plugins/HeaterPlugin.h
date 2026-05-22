#pragma once

#include <Arduino.h>
#include "../core/Plugin.h"
#include "../core/IHeaterDriver.h"
#include "../core/constants.h"
#include "../models/MachineState.h"

class HeaterPlugin : public Plugin, public IHeaterDriver {
public:
    const char* getName() const override { return "Heater"; }

    bool setup() override {
        pinMode(PIN_HEATER_SSR, OUTPUT);
        digitalWrite(PIN_HEATER_SSR, LOW);
        gState.heaterOn = false;

        // Listen for PID output changes
        bus().subscribe(EventType::PIDOutputChanged, [this](const Event& e) {
            if (gState.mode != OperatingMode::Idle) {
                setDuty((uint8_t)constrain(e.floatValue, 0.0f, 255.0f));
            }
        });

        // Listen for heater on/off commands
        bus().subscribe(EventType::HeaterStateChanged, [this](const Event& e) {
            if (e.boolValue) {
                enable();
            } else {
                disable();
            }
        });

        DEBUG_PRINTF("[Heater] SSR on GPIO %d\n", PIN_HEATER_SSR);
        return true;
    }

    void loop() override {
        // Thermal Watchdog override (001-thermal-watchdog T019).
        // When the watchdog latch is active, HeaterPlugin must NEVER
        // assert HIGH on the SSR, even if _active and a non-zero
        // _dutyCycle are still present. Same defense-in-depth pattern
        // as the existing Idle/PID-output guard (RB-06).
        if (gState.watchdogTripped) {
            digitalWrite(PIN_HEATER_SSR, LOW);
            gState.heaterOn = false;
            return;
        }

        if (!_active) {
            digitalWrite(PIN_HEATER_SSR, LOW);
            gState.heaterOn = false;
            return;
        }

        // Soft PWM: time-proportional control within a window
        uint32_t now = millis();
        uint32_t elapsed = now - _windowStart;

        if (elapsed >= _windowSize) {
            _windowStart = now;
            elapsed = 0;
        }

        // Map duty cycle (0-255) to window time
        uint32_t onTime = ((uint32_t)_dutyCycle * _windowSize) / 255;

        if (elapsed < onTime) {
            digitalWrite(PIN_HEATER_SSR, HIGH);
            gState.heaterOn = true;
        } else {
            digitalWrite(PIN_HEATER_SSR, LOW);
            gState.heaterOn = false;
        }
    }

    void enable() override { _active = true; }
    void disable() override {
        _active = false;
        _dutyCycle = 0;
        digitalWrite(PIN_HEATER_SSR, LOW);
        gState.heaterOn = false;
    }

    void setDuty(uint8_t duty) override { _dutyCycle = duty; }

    bool isActive() const override { return _active; }
    uint8_t getDutyCycle() const override { return _dutyCycle; }

private:
    bool _active = false;
    uint8_t _dutyCycle = 0;
    uint32_t _windowStart = 0;
    uint32_t _windowSize = 1000;    // 1 second PWM window
};
