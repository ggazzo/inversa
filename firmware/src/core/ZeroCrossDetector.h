#pragma once

#ifndef NATIVE_BUILD  // ZC detector requires Arduino interrupt APIs

#include <Arduino.h>

// Zero-cross detector singleton. Reads pulses from an opto-isolated ZC
// detector circuit on a GPIO line and exposes a half-cycle counter that
// HeaterPluginZC consumes to align SSR switching with mains zero-cross.
//
// Wiring assumption: detector produces ONE pulse per AC zero-cross
// (e.g. H11AA1 + rectifier feeding optocoupler open-collector output with
// a pull-up). Default edge = FALLING. At 60 Hz that yields 120 ISRs/s.
//
// Fault detection: if no ISR for ~3 half-periods (e.g. >25 ms @60 Hz),
// isFaulted() returns true so HeaterPluginZC can refuse to fire the SSR.
//
// Implementation lives in ZeroCrossDetector.cpp so the IRAM ISR ends up in
// a translation unit the Xtensa linker can place correctly (header-only
// IRAM functions trigger "dangerous relocation: l32r" link errors).
class ZeroCrossDetector {
public:
    static ZeroCrossDetector& instance();

    void begin(uint8_t pin, uint16_t mainsFreqHz = 60, int edgeMode = FALLING);

    uint32_t halfCycleCount() const { return _halfCycleCount; }
    bool     isFaulted() const;
    uint16_t mainsFreqHz() const { return _freq; }
    uint32_t halfPeriodUs() const { return _halfPeriodUs; }

private:
    ZeroCrossDetector() = default;
    static void IRAM_ATTR isrThunk(void* arg);

    uint8_t  _pin            = 0;
    uint16_t _freq           = 60;
    uint32_t _halfPeriodUs   = 8333;
    uint32_t _debounceUs     = 5833;
    uint32_t _faultTimeoutUs = 25000;
    volatile uint32_t _lastIsrUs      = 0;
    volatile uint32_t _halfCycleCount = 0;
};

#endif  // NATIVE_BUILD
