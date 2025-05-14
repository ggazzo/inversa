#include "NTC_Thermistor_CustomFormula_ESP32.h"
#include <Arduino.h> // For INFINITY, NAN
#include <math.h>    // For INFINITY, NAN

NTC_Thermistor_CustomFormula_ESP32::NTC_Thermistor_CustomFormula_ESP32(
    uint8_t adcPin,
    double seriesResistor,      // This is R_fixed for our formula
    double nominalResistance,
    double bCoefficient,
    double nominalTemperature,    // Celsius
    double referenceVoltage,    // This is V_in for our formula (in Volts)
    uint16_t adcResolution
) : NTC_Thermistor_ESP32(
    adcPin,                 // Pass to base
    seriesResistor,         // Pass R_fixed as referenceResistance to base
    nominalResistance,      // Pass to base
    bCoefficient,           // Pass to base
    nominalTemperature,     // Pass to base (expects Celsius)
    referenceVoltage,       // Pass to base
    adcResolution           // Pass to base
) {
    
}

double NTC_Thermistor_CustomFormula_ESP32::readResistance() {
    return this->referenceResistance * (this->adcResolution / readVoltage() - 1);
} 