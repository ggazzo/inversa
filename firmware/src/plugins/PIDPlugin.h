#pragma once

#include <Arduino.h>
#include "../core/Plugin.h"
#include "../core/constants.h"
#include "../core/NVSStorage.h"
#include "../core/ThermalCalc.h"
#include "../models/MachineState.h"

class PIDPlugin : public Plugin {
public:
    const char* getName() const override { return "PID"; }

    bool setup() override {
        // Load PID params from NVS, or use defaults
        NVSStorage& nvs = NVSStorage::instance();
        _kp = nvs.loadPIDKp(PID_KP_DEFAULT);
        _ki = nvs.loadPIDKi(PID_KI_DEFAULT);
        _kd = nvs.loadPIDKd(PID_KD_DEFAULT);
        _outputMin = PID_OUTPUT_MIN;
        _outputMax = PID_OUTPUT_MAX;
        _sampleTime = PID_SAMPLE_TIME_MS;

        gState.pidKp = _kp;
        gState.pidKi = _ki;
        gState.pidKd = _kd;
        
        if (nvs.hasPIDParams()) {
            DEBUG_PRINTF("[PID] Loaded from NVS: Kp=%.2f Ki=%.4f Kd=%.1f\n", _kp, _ki, _kd);
        }

        // Listen for setpoint changes (direct or ramped)
        bus().subscribe(EventType::SetpointChanged, [this](const Event& e) {
            // Only use direct setpoint if ramp is not active
            if (!gState.rampActive) {
                _setpoint = e.floatValue;
            }
        });

        // Listen for ramped setpoint (takes priority when ramping)
        bus().subscribe(EventType::RampedSetpointChanged, [this](const Event& e) {
            _setpoint = e.floatValue;
        });

        // Cache the cylinder surface area derived from volumeLiters +
        // vesselDiameter. Both inputs change only via NVS load or BLE
        // command (SettingsChanged event), so recomputing at 10 Hz on the
        // hot PID path was pure waste. Recompute lazily on SettingsChanged
        // and seed once here from the values loaded by main.cpp before
        // plugin setup().
        invalidateSurfaceArea();
        bus().subscribe(EventType::SettingsChanged, [this](const Event&) {
            invalidateSurfaceArea();
        });

        // Listen for PID param changes
        bus().subscribe(EventType::PIDParamsChanged, [this](const Event& e) {
            // Params sent as JSON string "kp,ki,kd"
            // Parsed elsewhere — just update from gState
            _kp = gState.pidKp;
            _ki = gState.pidKi;
            _kd = gState.pidKd;
            reset();
        });

        // Listen for temperature readings
        bus().subscribe(EventType::TemperatureRead, [this](const Event& e) {
            _input = e.floatValue;
        });

        DEBUG_PRINTF("[PID] Initialized Kp=%.2f Ki=%.4f Kd=%.1f\n", _kp, _ki, _kd);
        return true;
    }

    void loop() override {
        if (gState.mode == OperatingMode::Idle) {
            _output = 0;
            gState.pidOutput = 0;
            return;
        }

        uint32_t now = millis();
        uint32_t elapsed = now - _lastCompute;
        if (elapsed < _sampleTime) return;

        float error = _setpoint - _input;

        // Proportional
        float pTerm = _kp * error;

        // Integral (with anti-windup clamping)
        _integral += _ki * error * (elapsed / 1000.0f);
        _integral = constrain(_integral, _outputMin, _outputMax);

        // Derivative (on measurement to avoid derivative kick)
        float dInput = (_input - _lastInput) / (elapsed / 1000.0f);
        float dTerm = -_kd * dInput;

        // Feed-forward: compensate for heat loss
        float ffTerm = 0;
        if (_feedForwardEnabled && _setpoint > gState.ambientTemp) {
            float ffOutput = ThermalCalc::calculateFeedForward(
                _setpoint, _input, gState.heaterPowerWatts,
                gState.volumeLiters, gState.ambientTemp, surfaceArea(),
                gState.heatLossCoeff);
            ffTerm = ffOutput * _outputMax;
        }

        // Output = PID + Feed-forward
        _output = constrain(pTerm + _integral + dTerm + ffTerm, _outputMin, _outputMax);

        _lastInput = _input;
        _lastCompute = now;

        gState.pidOutput = _output;
        bus().publish(EventType::PIDOutputChanged, _output);
    }

    void setTunings(float kp, float ki, float kd, bool persist = false) {
        _kp = kp; _ki = ki; _kd = kd;
        gState.pidKp = kp;
        gState.pidKi = ki;
        gState.pidKd = kd;
        reset();
        
        if (persist) {
            NVSStorage::instance().savePIDParams(kp, ki, kd);
        }
    }
    
    void saveToNVS() {
        NVSStorage::instance().savePIDParams(_kp, _ki, _kd);
    }

    void reset() {
        _integral = 0;
        _lastInput = _input;
        _output = 0;
    }

    float getOutput() const { return _output; }
    float getSetpoint() const { return _setpoint; }

    // Enable/disable feed-forward
    void setFeedForward(bool enabled) { _feedForwardEnabled = enabled; }
    bool isFeedForwardEnabled() const { return _feedForwardEnabled; }

private:
    float _kp = 0, _ki = 0, _kd = 0;
    float _setpoint = 0;
    float _input = 0;
    float _output = 0;
    float _integral = 0;
    float _lastInput = 0;
    float _outputMin = 0, _outputMax = 255;
    uint32_t _sampleTime = 1000;
    uint32_t _lastCompute = 0;
    bool _feedForwardEnabled = true;  // Enable by default

    // Cached surface area + inputs used to compute it. Recomputed lazily
    // when the inputs change since the last query (handles both
    // SettingsChanged events and the boot-time load order where NVS
    // updates gState after our setup() ran).
    float _cachedSurfaceArea = 0;
    float _cachedVolumeLiters = 0;
    float _cachedVesselDiameter = 0;

    void invalidateSurfaceArea() {
        _cachedVolumeLiters = -1;
        _cachedVesselDiameter = -1;
    }

    float surfaceArea() {
        if (_cachedVolumeLiters    != gState.volumeLiters ||
            _cachedVesselDiameter  != gState.vesselDiameter) {
            _cachedVolumeLiters   = gState.volumeLiters;
            _cachedVesselDiameter = gState.vesselDiameter;
            _cachedSurfaceArea    = ThermalCalc::cylinderSurfaceArea(
                _cachedVolumeLiters, _cachedVesselDiameter);
        }
        return _cachedSurfaceArea;
    }
};
