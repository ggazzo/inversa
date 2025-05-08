#include "calc.h"
#include <Arduino.h>


// Function to calculate the time required to heat water
float calculateHeatingTime_seconds(float volumeLiters, float powerWatts, float initialTemperature, float finalTemperature) {
    // Constants
    const float specificHeatCapacity = 4186; // Specific heat capacity of water in J/kg°C

    // Convert volume to mass (1 liter of water = 1 kg)
    float massKg = volumeLiters;

    // Calculate the temperature change
    float deltaTemperature = finalTemperature - initialTemperature;

    // Calculate the amount of heat required (Q = m * c * ΔT)
    float heatRequired = massKg * specificHeatCapacity * deltaTemperature;

    // Calculate the time required (t = Q / P)
    float timeSeconds = heatRequired / powerWatts;

    return timeSeconds;
}

// Function to calculate the cooling constant k
float calculateCoolingConstant(float T0, float Tt, float Tamb, float t) {
    // Validate inputs to avoid division by zero or invalid calculations
    if (Tt <= Tamb || T0 <= Tamb || t <= 0) {
        return -1; // Indicate an error
    }

    // Calculate the fraction
    float fraction = (Tt - Tamb) / (T0 - Tamb);
    if (fraction <= 0) {
        return -1; // Indicate an error
    }

    // Calculate the constant k
    float k = -log(fraction) / t;

    return k;
}