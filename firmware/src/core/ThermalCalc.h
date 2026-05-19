#pragma once

#ifndef NATIVE_BUILD
  #include <Arduino.h>
#endif

#include <cmath>
#include <algorithm>

// ThermalCalc only needs M_PI, std::min, and a clamp. We use std:: variants
// so the namespace compiles cleanly under native test as well as Arduino.
#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

// ─── Thermal Calculation Utilities ──────────────────────────
// Functions for calculating heating time, heat loss, and related
// thermal properties for brewing operations.

namespace ThermalCalc {

    // Physical constants
    constexpr float SPECIFIC_HEAT_WATER = 4186.0f;  // J/(kg·K)
    constexpr float STEFAN_BOLTZMANN = 5.67e-8f;     // W/(m²·K⁴)
    constexpr float WATER_DENSITY = 1.0f;            // kg/L (approximation)

    // ─── Surface Area Calculation ───────────────────────────
    // Calculate surface area of a cylindrical vessel from volume and diameter
    // Returns total surface area in m² (sides + bottom, no lid)
    inline float cylinderSurfaceArea(float volumeLiters, float diameterMeters) {
        float radius = diameterMeters / 2.0f;
        float radiusSq = radius * radius;
        
        // Height from volume: V = π·r²·h → h = V / (π·r²)
        // Convert liters to m³: 1L = 0.001 m³
        float heightMeters = (volumeLiters * 0.001f) / (M_PI * radiusSq);
        
        // Surface area: sides + bottom (assuming open top or lid doesn't count)
        float sideArea = 2.0f * M_PI * radius * heightMeters;
        float bottomArea = M_PI * radiusSq;
        
        return sideArea + bottomArea;
    }

    // ─── Heat Loss Calculation (Convection only) ────────────
    // Calculate heat loss rate in Watts
    // Uses Newton's law of cooling: Q = h·A·ΔT
    //
    // @param liquidTemp     Current liquid temperature (°C)
    // @param ambientTemp    Ambient temperature (°C)
    // @param surfaceArea    Surface area in m²
    // @param heatTransferCoeff  Heat transfer coefficient (W/m²·K)
    //                           Typical values: 5-10 (still air), 10-25 (light breeze)
    // @return Heat loss in Watts
    inline float calculateHeatLoss(float liquidTemp, float ambientTemp, 
                                   float surfaceArea, float heatTransferCoeff = 10.0f) {
        float deltaT = liquidTemp - ambientTemp;
        return heatTransferCoeff * surfaceArea * deltaT;
    }

    // ─── Heat Loss with Radiation ───────────────────────────
    // More accurate but computationally heavier
    // Includes both convection and radiation losses
    //
    // @param emissivity  Surface emissivity (0.1-0.95, stainless ~0.3, oxidized ~0.8)
    // pow(x, 4) is ~200 cycles on ESP32; called in the PID feed-forward hot
    // path (~10 Hz). x*x*x*x folds to two muls, ~10 cycles.
    inline float pow4f(float x) {
        float x2 = x * x;
        return x2 * x2;
    }

    inline float calculateHeatLossWithRadiation(float liquidTemp, float ambientTemp,
                                                 float surfaceArea, float heatTransferCoeff = 10.0f,
                                                 float emissivity = 0.5f) {
        // Convection
        float Q_conv = calculateHeatLoss(liquidTemp, ambientTemp, surfaceArea, heatTransferCoeff);

        // Radiation (Stefan-Boltzmann law)
        float T_liquid_K = liquidTemp + 273.15f;
        float T_ambient_K = ambientTemp + 273.15f;
        float Q_rad = emissivity * STEFAN_BOLTZMANN * surfaceArea *
                      (pow4f(T_liquid_K) - pow4f(T_ambient_K));

        return Q_conv + Q_rad;
    }

    // ─── Heating Time Calculation ───────────────────────────
    // Calculate time to heat water from initial to final temperature
    // Accounts for heat losses during heating
    //
    // @param volumeLiters    Water volume in liters
    // @param powerWatts      Heater power in Watts
    // @param initialTemp     Starting temperature (°C)
    // @param finalTemp       Target temperature (°C)
    // @param efficiency      Heater efficiency (0.0-1.0, typically 0.85-0.95)
    // @param heatLossWatts   Average heat loss during heating (optional)
    // @return Time in seconds
    inline float calculateHeatingTime(float volumeLiters, float powerWatts,
                                      float initialTemp, float finalTemp,
                                      float efficiency = 0.90f, float heatLossWatts = 0.0f) {
        if (finalTemp <= initialTemp) return 0.0f;
        
        float massKg = volumeLiters * WATER_DENSITY;
        float deltaT = finalTemp - initialTemp;
        
        // Energy required: Q = m·c·ΔT
        float energyJoules = massKg * SPECIFIC_HEAT_WATER * deltaT;
        
        // Effective power = heater power × efficiency - heat loss
        float effectivePower = (powerWatts * efficiency) - heatLossWatts;
        if (effectivePower <= 0) return -1.0f;  // Cannot heat!
        
        // Time = Energy / Power
        return energyJoules / effectivePower;
    }

    // ─── Heating Time with Dynamic Heat Loss ────────────────
    // More accurate: calculates average heat loss during heating
    //
    // @param ambientTemp     Ambient temperature for heat loss calculation
    // @param surfaceArea     Vessel surface area in m²
    // @param heatTransferCoeff  Heat transfer coefficient (W/m²·K)
    inline float calculateHeatingTimeWithLoss(float volumeLiters, float powerWatts,
                                               float initialTemp, float finalTemp,
                                               float ambientTemp, float surfaceArea,
                                               float efficiency = 0.90f,
                                               float heatTransferCoeff = 10.0f) {
        if (finalTemp <= initialTemp) return 0.0f;
        
        // Calculate average temperature during heating
        float avgTemp = (initialTemp + finalTemp) / 2.0f;
        
        // Calculate average heat loss
        float avgHeatLoss = calculateHeatLoss(avgTemp, ambientTemp, surfaceArea, heatTransferCoeff);
        
        return calculateHeatingTime(volumeLiters, powerWatts, initialTemp, finalTemp,
                                    efficiency, avgHeatLoss);
    }

    // ─── Minimum Power to Maintain Temperature ──────────────
    // Calculate minimum heater power needed to maintain a setpoint
    // (compensate for heat losses)
    //
    // @return Minimum power in Watts
    inline float calculateMinimumPower(float setpointTemp, float ambientTemp,
                                       float surfaceArea, float heatTransferCoeff = 10.0f) {
        return calculateHeatLoss(setpointTemp, ambientTemp, surfaceArea, heatTransferCoeff);
    }

    // ─── Feed-Forward Power ─────────────────────────────────
    // Calculate suggested heater output for PID feed-forward
    // Returns value 0.0-1.0 representing duty cycle
    //
    // @param targetTemp      Target temperature
    // @param currentTemp     Current temperature  
    // @param maxPower        Maximum heater power in Watts
    // @param volumeLiters    Water volume
    // @param ambientTemp     Ambient temperature
    // @param surfaceArea     Vessel surface area
    inline float calculateFeedForward(float targetTemp, float currentTemp,
                                      float maxPower, float volumeLiters,
                                      float ambientTemp, float surfaceArea,
                                      float heatTransferCoeff = 10.0f) {
        // Power needed to overcome heat loss at target temp
        float heatLossPower = calculateHeatLoss(targetTemp, ambientTemp, surfaceArea, heatTransferCoeff);
        
        // Additional power for heating (if below target)
        float heatingPower = 0.0f;
        if (currentTemp < targetTemp) {
            float deltaT = targetTemp - currentTemp;
            float massKg = volumeLiters * WATER_DENSITY;
            // Energy to heat 1°C per second
            float powerPerDegree = massKg * SPECIFIC_HEAT_WATER;
            // Proportional power based on error (cap at 10°C error)
            float error = std::min(deltaT, 10.0f);
            heatingPower = (powerPerDegree * error) / 60.0f;  // Spread over 1 minute
        }
        
        float totalPower = heatLossPower + heatingPower;
        return std::clamp(totalPower / maxPower, 0.0f, 1.0f);
    }

    // ─── Cooling Time Estimation ────────────────────────────
    // Estimate time to cool from current temp to target temp
    // Uses Newton's law of cooling
    //
    // @param coolingConstant  Cooling rate constant (1/s), or 0 to estimate
    inline float calculateCoolingTime(float currentTemp, float targetTemp,
                                      float ambientTemp, float volumeLiters,
                                      float surfaceArea, float heatTransferCoeff = 10.0f) {
        if (targetTemp >= currentTemp || targetTemp < ambientTemp) return 0.0f;
        
        // Estimate cooling constant k from heat transfer
        // k = h·A / (m·c)
        float massKg = volumeLiters * WATER_DENSITY;
        float k = (heatTransferCoeff * surfaceArea) / (massKg * SPECIFIC_HEAT_WATER);
        
        // Newton's law: T(t) = Tamb + (T0 - Tamb)·e^(-kt)
        // Solving for t: t = -ln((T - Tamb)/(T0 - Tamb)) / k
        float fraction = (targetTemp - ambientTemp) / (currentTemp - ambientTemp);
        if (fraction <= 0) return -1.0f;
        
        return -log(fraction) / k;
    }

}  // namespace ThermalCalc
