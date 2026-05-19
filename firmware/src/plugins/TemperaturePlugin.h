#pragma once

#include <Arduino.h>
#include <SimpleKalmanFilter.h>
#include "../core/Plugin.h"
#include "../core/constants.h"
#include "../models/MachineState.h"

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

        float raw = readNTC();
        if (raw < TEMP_MIN || raw > TEMP_MAX) {
            gState.tempSensorOk = false;
            bus().publish(EventType::TemperatureError);
            return;
        }

        float filtered = _kalman.updateEstimate(raw);
        gState.currentTemp = filtered;
        gState.tempSensorOk = true;
        bus().publish(EventType::TemperatureRead, filtered);
    }

    float getRawTemp() const { return _lastRaw; }
    float getFilteredTemp() const { return gState.currentTemp; }

private:
    SimpleKalmanFilter _kalman{KALMAN_MEASURE_ERROR, KALMAN_ESTIMATE_ERROR, KALMAN_PROCESS_NOISE};
    uint32_t _lastRead = 0;
    float _lastRaw = 0.0f;

    float readNTC() {
        int adcValue = analogRead(PIN_NTC);
        if (adcValue <= 0 || adcValue >= NTC_ADC_RESOLUTION) {
            return TEMP_ERROR_VALUE;
        }

        // Voltage divider: 3.3V → NTC → ADC node → R_ref → GND
        // (NTC on top, matches the physical Inversa board — confirmed
        // against develop's NTC_Thermistor_CustomFormula_ESP32 lib,
        // which uses the same formula).
        // V_adc = V_supply · R_ref / (R_ntc + R_ref)
        // ⇒ R_ntc = R_ref · (ADC_MAX − adc) / adc
        float resistance = (float)NTC_REFERENCE_RESISTANCE * (NTC_ADC_RESOLUTION - adcValue) / adcValue;

        // Steinhart-Hart simplified (B-parameter equation)
        float steinhart = resistance / (float)NTC_NOMINAL_RESISTANCE;   // R/R0
        steinhart = log(steinhart);                                      // ln(R/R0)
        steinhart /= (float)NTC_B_COEFFICIENT;                          // ln(R/R0) / B
        steinhart += 1.0f / ((float)NTC_NOMINAL_TEMPERATURE + 273.15f); // + 1/T0
        steinhart = 1.0f / steinhart;                                    // invert
        steinhart -= 273.15f;                                            // to Celsius

        _lastRaw = steinhart;
        return steinhart;
    }
};
