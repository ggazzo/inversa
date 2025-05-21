#include "outputControl.h"
#include <Arduino.h>
// #include <sTune.h>
#include "Components/Settings.h"

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#define LOW_TEMP_BAND 15 // degrees
#define HIGH_TEMP_BAND 1 // degrees


extern Settings settings;

#define MASS 10.0         // Mass of water in kg (10L of water)
#define CP 4186.0         // Specific heat capacity of water in J/kg°C
#define AMBIENT_TEMP 25.0 // Ambient temperature in °C
#define SURFACE_AREA 1.0  // Surface area of the container in m²
#define HEAT_TRANSFER_COEFF 10.0 // Heat transfer coefficient in W/m²°C

#define TEMP_AMBIENT 25

extern MachineState state;

PID_ATune pid_atune(&state.current_temperature_c, &state.output_val);


// float Input, Output, Setpoint = 80, Kp, Ki, Kd; // sTune
// sTune tuner = sTune(&Input, &Output, tuner.ZN_PID, tuner.directIP, tuner.printOFF);


struct HeatLossParams {
  float diameter;     // Diameter of the vessel (m)
  float height;       // Height of the vessel (m)
  float thickness;    // Wall thickness (m)
  float thermalConductivity;  // Thermal conductivity of vessel material (W/mK)
  float emissivity;   // Emissivity of vessel surface (0.3–0.95)
  float convectionCoeff; // Convection heat transfer coefficient (W/m²K)
};

float calculateSurfaceAreaFromVolume(float volume, float diameter, bool isCovered = false) {
  float radius = diameter / 2.0;
  
  // Calculate height from volume
  float height = volume / (M_PI * radius * radius);
  
  // Calculate surface area
  float sideArea = 2 * M_PI * radius * height;
  float bottomArea = M_PI * radius * radius;
  
  // Add top area if uncovered
  if (!isCovered) {
      return sideArea + 2 * bottomArea; // Includes top and bottom
  } else {
      return sideArea + bottomArea; // Only side and bottom
  }
}


float calculateHeatLoss(float liquidTemp, float ambientTemp, HeatLossParams params) {
  #define STEFAN_BOLTZMANN 5.67e-8  // W/m²K⁴
  float radius = params.diameter / 2.0;
  float surfaceArea = 2 * PI * radius * params.diameter + 2 * PI * sq(radius);  // Total surface area

  // Convective heat loss (W)
  float Q_conv = params.convectionCoeff * surfaceArea * (liquidTemp - ambientTemp);

  // Radiative heat loss (W)
  float T_l = liquidTemp + 273.15;  // Convert to Kelvin
  float T_a = ambientTemp + 273.15;
  float Q_rad = params.emissivity * STEFAN_BOLTZMANN * surfaceArea * (pow(T_l, 4) - pow(T_a, 4));

  return Q_conv + Q_rad;  // Total heat loss in watts
}



// Function to calculate heat loss (P_loss) in Watts
double calculateHeatLoss(double systemTemp, double ambientTemp, double surfaceArea, double heatTransferCoeff) {
  // Heat loss = h * A * (T_ambient - T_system)
  return heatTransferCoeff * surfaceArea * (ambientTemp - systemTemp);
}

float cylinderSurfaceArea(float radius, float height) {
    return (2 * PI * radius * radius) + (2 * PI * radius * height);
}


double feedForward(double targetTemp, double currentTemp, double maxPower, HeatLossParams params) {
    double deltaT = targetTemp - currentTemp;
    double energyNeeded = MASS * CP * deltaT;
    double powerRequired = (energyNeeded / 1000) + calculateHeatLoss(currentTemp, TEMP_AMBIENT, params);
    return constrain(powerRequired / maxPower, 0, 1) * maxPower;
}


float calculateMinimumPower(float volume, float surfaceArea, float T_set, float T_ambient, float U = 10.0) {
  // U (W/m²·K) depends on insulation, default value assumes moderate insulation
  float P_maintain = U * surfaceArea * (T_set - T_ambient);
  return P_maintain; // Minimum watts required
}