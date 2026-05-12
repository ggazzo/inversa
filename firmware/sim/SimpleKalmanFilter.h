#pragma once
// SimpleKalmanFilter stub — the simulator's ThermalSim produces a clean
// temperature signal, so we pass-through instead of running a real filter.

class SimpleKalmanFilter {
public:
    SimpleKalmanFilter(float /*mea_e*/, float /*est_e*/, float /*q*/) {}
    float updateEstimate(float mea) { return mea; }
};
