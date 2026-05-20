#pragma once

#include <Arduino.h>
#include <cmath>
#include <SimpleKalmanFilter.h>
#include "../core/Plugin.h"
#include "../core/constants.h"
#include "../models/MachineState.h"
#ifdef HIL_BUILD
#include "../hil/HilState.h"
#endif

class TemperaturePlugin : public Plugin {
public:
    const char* getName() const override { return "Temperature"; }

    bool setup() override {
        analogReadResolution(12);
        pinMode(PIN_NTC, INPUT);
        DEBUG_PRINTLN("[Temperature] NTC sensor initialized");
        return true;
    }

    void loop() override {
        uint32_t now = millis();
        if (now - _lastRead < LOOP_INTERVAL_MS) return;
        _lastRead = now;

        float raw;
#ifdef HIL_BUILD
        // HIL seam: when the harness has injected an NTC override, skip the
        // analogRead/Steinhart-Hart path entirely and feed the injected °C
        // into the Kalman filter as if it were a freshly-converted ADC
        // sample. ntcBypassKalman=true also short-circuits the filter so a
        // step input lands in gState in one tick (useful for the overtemp
        // trip-latency scenario).
        if (hil::g_hil.ntcActive) {
            raw = hil::g_hil.ntcCelsius;
            _lastRaw = raw;
            if (raw < TEMP_MIN || raw > TEMP_MAX) {
                gState.tempSensorOk = false;
                bus().publish(EventType::TemperatureError);
                return;
            }
            if (hil::g_hil.ntcBypassKalman) {
                float v = gState.tempCalSlope * raw + gState.tempCalOffset;
                gState.currentTemp = v;
                gState.tempSensorOk = true;
                bus().publish(EventType::TemperatureRead, v);
                return;
            }
        } else {
            raw = readNTC();
        }
#else
        raw = readNTC();
#endif
        if (raw < TEMP_MIN || raw > TEMP_MAX) {
            gState.tempSensorOk = false;
            bus().publish(EventType::TemperatureError);
            return;
        }

        float filtered = _kalman.updateEstimate(raw);
        // Apply the user-configured linear calibration AFTER the Kalman
        // filter so the filter still smooths the raw sensor signal
        // (operating on the calibrated value would let a fresh offset
        // edit get interpreted as a slope change for ~1 s).
        filtered = gState.tempCalSlope * filtered + gState.tempCalOffset;
        gState.currentTemp = filtered;
        gState.tempSensorOk = true;
        bus().publish(EventType::TemperatureRead, filtered);
    }

    float getRawTemp() const { return _lastRaw; }
    float getFilteredTemp() const { return gState.currentTemp; }

private:
    // Steinhart-Hart B-parameter precomputed constants. The reference
    // resistor and the 1/T0 offset are compile-time invariants — the old
    // code recomputed them as doubles every ADC sample (~10 Hz). Hoisting
    // them as `constexpr float` lets the compiler fold the arithmetic and
    // keeps the entire NTC math single-precision on the ESP32 FPU.
    static constexpr float R_REF_F          = (float)NTC_REFERENCE_RESISTANCE;
    static constexpr float R_NOM_F          = (float)NTC_NOMINAL_RESISTANCE;
    static constexpr float INV_B            = 1.0f / (float)NTC_B_COEFFICIENT;
    static constexpr float INV_T0_K         = 1.0f / ((float)NTC_NOMINAL_TEMPERATURE + 273.15f);
    static constexpr float ADC_MAX_F        = (float)NTC_ADC_RESOLUTION;

    SimpleKalmanFilter _kalman{KALMAN_MEASURE_ERROR, KALMAN_ESTIMATE_ERROR, KALMAN_PROCESS_NOISE};
    uint32_t _lastRead = 0;
    float _lastRaw = 0.0f;

    float readNTC() {
        int adcValue = analogRead(PIN_NTC);
        if (adcValue <= 0 || adcValue >= NTC_ADC_RESOLUTION) {
            return TEMP_ERROR_VALUE;
        }

        const float adcF = (float)adcValue;

        // Topology selected at compile time via NTC_HIGH_SIDE
        // (constants.h, override per env via -DNTC_HIGH_SIDE=0).
        //   NTC_HIGH_SIDE=1: 3.3V → NTC → ADC → R_ref → GND
        //     R_ntc = R_ref · (ADC_MAX − adc) / adc
        //   NTC_HIGH_SIDE=0: 3.3V → R_ref → ADC → NTC → GND
        //     R_ntc = R_ref · adc / (ADC_MAX − adc)
#if NTC_HIGH_SIDE
        float resistance = R_REF_F * (ADC_MAX_F - adcF) / adcF;
#else
        float resistance = R_REF_F * adcF / (ADC_MAX_F - adcF);
#endif

        // Steinhart-Hart simplified (B-parameter equation), single
        // precision throughout. `logf` is the ~80-cycle float variant —
        // `log` was the ~200-cycle double variant the ESP32 has to emulate
        // (no hard double on the FPU).
        float kelvin_inv = logf(resistance / R_NOM_F) * INV_B + INV_T0_K;
        float celsius    = 1.0f / kelvin_inv - 273.15f;

        _lastRaw = celsius;
        return celsius;
    }
};
