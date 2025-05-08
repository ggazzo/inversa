// Function to calculate the time required to heat water
#ifndef CALC_H
#define CALC_H
#define SECONDS_TO_MINUTES(x) ((x) / 60)
#define MILLIS_TO_SECONDS(x) ((x) / 1000)
float calculateHeatingTime_seconds(float volumeLiters, float powerWatts, float initialTemperature, float finalTemperature);
// Function to calculate the cooling constant k
float calculateCoolingConstant(float T0, float Tt, float Tamb, float t);
#endif