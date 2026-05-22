#pragma once

#include <Arduino.h>
#include "../core/Plugin.h"
#include "../core/IHeaterDriver.h"
#include "../core/ZeroCrossDetector.h"
#include "../core/constants.h"
#include "../models/MachineState.h"

// Burst-fire heater driver synced to mains zero-cross.
//
// Replaces the soft-PWM HeaterPlugin when a ZC detector circuit is wired in.
// Within a window of N half-cycles, the SSR is held HIGH for the first
// onHalfCycles = duty * N / 255 half-cycles and LOW for the rest. Because
// zero-cross SSRs only switch at the next mains ZC, alignment is automatic;
// this plugin just gates the input.
//
// Default window: 120 half-cycles (≈1 s @60 Hz, ≈1.2 s @50 Hz) →
// resolution 1/120 ≈ 0.83 %.
//
// Safety: same defense-in-depth as HeaterPlugin (watchdog override forces
// SSR LOW). Additionally, if the ZC detector reports a fault (signal lost),
// this plugin refuses to fire and isFaulted() returns true.
class HeaterPluginZC : public Plugin, public IHeaterDriver {
public:
    HeaterPluginZC(uint8_t ssrPin,
                   uint8_t zcPin,
                   uint16_t mainsFreqHz = 60,
                   uint8_t burstWindowHalfCycles = 120)
        : _ssrPin(ssrPin),
          _zcPin(zcPin),
          _mainsFreqHz(mainsFreqHz ? mainsFreqHz : 60),
          _burstWindow(burstWindowHalfCycles ? burstWindowHalfCycles : 120) {}

    const char* getName() const override { return "HeaterZC"; }

    bool setup() override {
        pinMode(_ssrPin, OUTPUT);
        digitalWrite(_ssrPin, LOW);
        gState.heaterOn = false;

        ZeroCrossDetector::instance().begin(_zcPin, _mainsFreqHz);

        bus().subscribe(EventType::PIDOutputChanged, [this](const Event& e) {
            if (gState.mode != OperatingMode::Idle) {
                setDuty((uint8_t)constrain(e.floatValue, 0.0f, 255.0f));
            }
        });

        bus().subscribe(EventType::HeaterStateChanged, [this](const Event& e) {
            if (e.boolValue) {
                enable();
            } else {
                disable();
            }
        });

        DEBUG_PRINTF("[HeaterZC] SSR=GPIO%u ZC=GPIO%u freq=%uHz window=%u half-cycles\n",
                     _ssrPin, _zcPin, _mainsFreqHz, _burstWindow);
        return true;
    }

    void loop() override {
        // Thermal Watchdog override: same contract as HeaterPlugin.
        if (gState.watchdogTripped) {
            digitalWrite(_ssrPin, LOW);
            gState.heaterOn = false;
            return;
        }

        // ZC signal lost → fail safe.
        if (ZeroCrossDetector::instance().isFaulted()) {
            digitalWrite(_ssrPin, LOW);
            gState.heaterOn = false;
            _faulted = true;
            return;
        }
        _faulted = false;

        if (!_active) {
            digitalWrite(_ssrPin, LOW);
            gState.heaterOn = false;
            return;
        }

        // Burst-fire with Bresenham-style pattern spreading. Distributes the
        // ON half-cycles evenly across the burst window instead of grouping
        // them in a contiguous block, which kills the sub-50 Hz flicker that
        // a contiguous burst causes on lights sharing the same AC phase.
        //
        // For each window position `pos`, ON if the integer-divided cumulative
        // ON count steps forward between pos and pos+1:
        //   on(pos) = floor((pos+1) * M / N) - floor(pos * M / N) != 0
        // where M = onCycles, N = _burstWindow. Total ON count over a window
        // is exactly M, just spread evenly.
        uint32_t hc       = ZeroCrossDetector::instance().halfCycleCount();
        uint16_t pos      = (uint16_t)(hc % (uint32_t)_burstWindow);
        uint16_t onCycles = ((uint16_t)_dutyCycle * _burstWindow) / 255;

        uint32_t a = ((uint32_t)pos       * onCycles) / _burstWindow;
        uint32_t b = ((uint32_t)(pos + 1) * onCycles) / _burstWindow;
        bool on = (b > a);
        digitalWrite(_ssrPin, on ? HIGH : LOW);
        gState.heaterOn = on;
    }

    void enable() override { _active = true; }
    void disable() override {
        _active = false;
        _dutyCycle = 0;
        digitalWrite(_ssrPin, LOW);
        gState.heaterOn = false;
    }

    void setDuty(uint8_t duty) override { _dutyCycle = duty; }

    bool isActive() const override { return _active; }
    uint8_t getDutyCycle() const override { return _dutyCycle; }
    bool isFaulted() const override { return _faulted; }

    // Tunable at runtime (NVS-backed; CommandHandler::REQ_HEATER_CONFIG also writes here).
    void setBurstWindow(uint8_t halfCycles) override {
        if (halfCycles >= 10) _burstWindow = halfCycles;
    }
    uint8_t getBurstWindow() const override { return _burstWindow; }

private:
    uint8_t  _ssrPin;
    uint8_t  _zcPin;
    uint16_t _mainsFreqHz;
    uint8_t  _burstWindow;          // in half-cycles
    bool     _active   = false;
    bool     _faulted  = false;
    uint8_t  _dutyCycle = 0;
};
