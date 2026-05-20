#pragma once
#include <cstdint>
#include <atomic>
#include <cmath>

// ThermalSim — closed-loop thermodynamics for the brewing simulator.
//
// The firmware HeaterPlugin calls digitalWrite(PIN_HEATER_SSR, HIGH|LOW).
// sim/Arduino.h routes that to ThermalSim::setHeater(). Each sim tick we
// integrate the temperature ODE forward by dt (default 100 ms):
//
//   m * c_water * dT/dt = P_heater(t) * efficiency  -  h * A * (T - T_amb)
//
// where m = volumeLiters * 1 kg/L (water density), c_water = 4186 J/(kg·K),
// P_heater is full power when SSR is HIGH and 0 when LOW (soft-PWM gives
// the right time-average over its 1 s window), h is the heat-loss
// coefficient (W/m²K) and A is the vessel surface area.
//
// TemperaturePlugin still calls analogRead(PIN_NTC); sim/Arduino.h routes
// it here, which converts the simulated °C back to a 12-bit ADC value via
// the inverse of the Steinhart-Hart B-equation TemperaturePlugin uses.

struct ThermalSimParams {
    float volumeLiters       = 25.0f;
    float heaterPowerW       = 3000.0f;
    float efficiency         = 0.90f;
    float ambientTempC       = 22.0f;
    float diameterM          = 0.35f;
    // Dual coefficient so the sim can validate LossTune against a known
    // truth in either lid mode. Single-coeff callers can set both fields
    // to the same value.
    float heatLossCoeffLidOn  = 8.0f;
    float heatLossCoeffLidOff = 12.0f;
    // Selects which coefficient drives the cooling term. Default ON to
    // match historical sim defaults (insulated kettle).
    bool  lidOn               = true;
    float initialTempC        = 22.0f;
};

class ThermalSim {
public:
    using Params = ThermalSimParams;

    static void configure(const Params& p) {
        _p = p;
        _tempC.store(p.initialTempC);
        _radius      = p.diameterM / 2.0f;
        // V = π r² h  →  h = V / (π r²);  in m³ (V_L * 0.001)
        const float heightM = (p.volumeLiters * 0.001f) / (M_PI * _radius * _radius);
        _surfaceArea = 2.0f * M_PI * _radius * heightM + M_PI * _radius * _radius;
    }

    // ── Heater hook (called by sim/Arduino.h digitalWrite) ──────────
    static void setHeater(bool on) { _heaterOn.store(on); }
    static bool heaterOn()         { return _heaterOn.load(); }

    static void setPump(bool on)   { _pumpOn.store(on); }
    static bool pumpOn()           { return _pumpOn.load(); }

    // Runtime lid toggle — bench code / tests can flip mid-simulation.
    static void setLid(bool lidOn) { _p.lidOn = lidOn; }
    static bool lidOn()            { return _p.lidOn; }

    // ── ODE integration ─────────────────────────────────────────────
    // dt in seconds (sim driver passes 0.1 for a 100 ms tick).
    static void tick(float dt) {
        const float T = _tempC.load();
        const float P_in  = _heaterOn.load() ? (_p.heaterPowerW * _p.efficiency) : 0.0f;
        const float h     = _p.lidOn ? _p.heatLossCoeffLidOn : _p.heatLossCoeffLidOff;
        const float P_out = h * _surfaceArea * (T - _p.ambientTempC);
        const float massKg = _p.volumeLiters;  // density 1 kg/L
        const float C_WATER = 4186.0f;
        const float dT = (P_in - P_out) / (massKg * C_WATER) * dt;
        _tempC.store(T + dT);
    }

    static float tempC() { return _tempC.load(); }

    // ── ADC synthesis (inverse Steinhart-Hart) ──────────────────────
    // Returns a 12-bit ADC value matching TemperaturePlugin::readNTC().
    // NTC params copied from firmware/src/core/constants.h to avoid pulling
    // the full header here (and the circular include it would create).
    static int adcRead() {
        constexpr float R0          = 10000.0f;
        constexpr float R_REF       = 10000.0f;
        constexpr float T0_KELVIN   = 25.0f + 273.15f;
        constexpr float BETA        = 3950.0f;
        constexpr int   ADC_MAX     = 4095;

        const float T_K = _tempC.load() + 273.15f;
        // From R = R0 * exp(B * (1/T - 1/T0))
        const float R   = R0 * std::exp(BETA * (1.0f / T_K - 1.0f / T0_KELVIN));
        // TemperaturePlugin computes R = R_REF * adc / (ADC_MAX - adc).
        // Invert: adc = ADC_MAX * R / (R + R_REF).
        const float adc = (float)ADC_MAX * R / (R + R_REF);
        int rounded = (int)(adc + 0.5f);
        if (rounded < 1)        rounded = 1;       // avoid the "0 = error" branch
        if (rounded >= ADC_MAX) rounded = ADC_MAX - 1;
        return rounded;
    }

private:
    inline static ThermalSimParams _p{};
    inline static std::atomic<float> _tempC{22.0f};
    inline static std::atomic<bool>  _heaterOn{false};
    inline static std::atomic<bool>  _pumpOn{false};
    inline static float _radius      = 0.175f;
    inline static float _surfaceArea = 0.3f;
};
