#ifndef NTC_THERMISTOR_CUSTOM_FORMULA_ESP32_H
#define NTC_THERMISTOR_CUSTOM_FORMULA_ESP32_H

#include <NTC_Thermistor.h> // From the library in .pio/libdeps

class NTC_Thermistor_CustomFormula_ESP32 : public NTC_Thermistor_ESP32 {
public:
    NTC_Thermistor_CustomFormula_ESP32(
        uint8_t adcPin,
        double seriesResistor,      // R_fixed (Ohms)
        double nominalResistance,   // R0 (Ohms at T0)
        double bCoefficient,        // B-coefficient
        double nominalTemperature = 25.0, // T0 (Celsius)
        double referenceVoltage = 3.3,    // V_in (Volts)
        uint16_t adcResolution = 4095   // Max ADC reading
    );

    // Override the virtual function from the base NTC_Thermistor class
    virtual double readResistance() override;
};

#endif // NTC_THERMISTOR_CUSTOM_FORMULA_ESP32_H 