#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/constants.h"
#include "../models/MachineState.h"
#include "../protocol/protocol.h"
#include "PIDPlugin.h"

// ─── Auto-Tune Plugin ───────────────────────────────────────
// Implements relay feedback auto-tuning (Ziegler-Nichols method)
// to automatically calculate optimal PID parameters.
//
// Process:
// 1. User sets target temperature and starts auto-tune
// 2. Relay oscillates between full power and off
// 3. System measures oscillation period and amplitude
// 4. Calculates Kp, Ki, Kd using Ziegler-Nichols formulas
//
// The relay feedback method works by:
// - Turning heater ON when temp < setpoint - hysteresis
// - Turning heater OFF when temp > setpoint + hysteresis
// - Measuring the resulting oscillation characteristics

class AutoTunePlugin : public Plugin {
public:
    AutoTunePlugin(MachineState& state, PIDPlugin& pid) 
        : _state(state), _pid(pid) {}

    const char* getName() const override { return "AutoTune"; }

    bool setup() override {
        // Subscribe to temperature readings
        bus().subscribe(EventType::TemperatureRead, [this](const Event& e) {
            if (_running) {
                processSample(e.floatValue);
            }
        });

        DEBUG_PRINTLN("[AutoTune] Plugin initialized");
        return true;
    }

    void loop() override {
        if (!_running) return;

        uint32_t now = millis();

        // Check timeout
        if (now - _startTime > AUTOTUNE_TIMEOUT_MS) {
            fail("Timeout - no oscillation detected");
            return;
        }

        // Send progress updates periodically
        if (now - _lastProgressUpdate > 5000) {
            _lastProgressUpdate = now;
            sendProgress();
        }
    }

    // ── Public API ───────────────────────────────────────────

    void start(float setpoint) {
        if (_running) {
            DEBUG_PRINTLN("[AutoTune] Already running");
            return;
        }

        _setpoint = setpoint;
        _running = true;
        _startTime = millis();
        _lastProgressUpdate = _startTime;
        _cycleCount = 0;
        _peakCount = 0;
        _lastPeakTime = 0;
        _periodSum = 0;
        _amplitudeSum = 0;
        _peakHigh = -1000;
        _peakLow = 1000;
        _outputState = false;
        _lastSampleTime = 0;
        _lookbackIndex = 0;
        
        // Initialize lookback buffer
        for (int i = 0; i < LOOKBACK_SIZE; i++) {
            _lookbackBuffer[i] = 0;
        }

        _state.autoTuneActive = true;
        _state.autoTuneProgress = 0;
        setStr(_state.autoTuneStatus, "Iniciando...");
        _state.mode = OperatingMode::Tuning;

        // Start with heater ON if below setpoint
        updateOutput(_state.currentTemp);

        DEBUG_PRINTF("[AutoTune] Started at setpoint %.1f°C\n", _setpoint);
        bus().publish(EventType::AutoTuneStarted);
        sendProgress();
    }

    void stop() {
        if (!_running) return;

        _running = false;
        _state.autoTuneActive = false;
        _state.autoTuneProgress = 0;
        setStr(_state.autoTuneStatus, "Cancelado");
        _state.mode = OperatingMode::Idle;

        // Turn off heater
        bus().publish(EventType::HeaterStateChanged, false);

        DEBUG_PRINTLN("[AutoTune] Stopped by user");
        sendProgress();
    }

    bool isRunning() const { return _running; }

private:
    static constexpr int LOOKBACK_SIZE = 50;

    MachineState& _state;
    PIDPlugin& _pid;

    bool _running = false;
    float _setpoint = 0;
    uint32_t _startTime = 0;
    uint32_t _lastProgressUpdate = 0;
    uint32_t _lastSampleTime = 0;

    // Relay state
    bool _outputState = false;

    // Peak detection
    float _peakHigh = -1000;
    float _peakLow = 1000;
    uint32_t _lastPeakTime = 0;
    int _peakCount = 0;
    int _cycleCount = 0;

    // Oscillation measurements
    float _periodSum = 0;
    float _amplitudeSum = 0;

    // Lookback buffer for peak detection
    float _lookbackBuffer[LOOKBACK_SIZE];
    int _lookbackIndex = 0;

    void processSample(float temp) {
        uint32_t now = millis();

        // Sample at fixed interval
        if (now - _lastSampleTime < AUTOTUNE_SAMPLE_TIME_MS) return;
        _lastSampleTime = now;

        // Store in lookback buffer
        _lookbackBuffer[_lookbackIndex] = temp;
        _lookbackIndex = (_lookbackIndex + 1) % LOOKBACK_SIZE;

        // Update relay output
        bool previousState = _outputState;
        updateOutput(temp);

        // Detect peaks when relay switches
        if (_outputState != previousState) {
            detectPeak(temp, now, _outputState);
        }

        // Track overall min/max
        if (temp > _peakHigh) _peakHigh = temp;
        if (temp < _peakLow) _peakLow = temp;

        // Check if we have enough cycles
        if (_cycleCount >= AUTOTUNE_MIN_CYCLES) {
            calculatePID();
        }

        // Safety: stop if too many cycles
        if (_cycleCount >= AUTOTUNE_MAX_CYCLES) {
            fail("Max cycles reached without stable oscillation");
        }
    }

    void updateOutput(float temp) {
        // Relay logic with hysteresis (noise band)
        if (temp > _setpoint + AUTOTUNE_NOISE_BAND) {
            if (_outputState) {
                _outputState = false;
                bus().publish(EventType::HeaterStateChanged, false);
                gState.pidOutput = 0;
            }
        } else if (temp < _setpoint - AUTOTUNE_NOISE_BAND) {
            if (!_outputState) {
                _outputState = true;
                bus().publish(EventType::HeaterStateChanged, true);
                gState.pidOutput = AUTOTUNE_STEP_OUTPUT;
            }
        }
    }

    void detectPeak(float temp, uint32_t now, bool risingEdge) {
        if (_lastPeakTime == 0) {
            _lastPeakTime = now;
            return;
        }

        // Calculate period (full cycle = 2 half-periods)
        uint32_t halfPeriod = now - _lastPeakTime;
        _lastPeakTime = now;

        _peakCount++;

        // Every 2 peaks = 1 complete cycle
        if (_peakCount >= 2) {
            _cycleCount = _peakCount / 2;

            // Accumulate period (convert to seconds)
            _periodSum += (halfPeriod * 2) / 1000.0f;

            // Accumulate amplitude
            float amplitude = _peakHigh - _peakLow;
            _amplitudeSum += amplitude;

            // Reset peak tracking for next cycle
            _peakHigh = temp;
            _peakLow = temp;

            // Update progress
            _state.autoTuneProgress = min(90, (_cycleCount * 100) / AUTOTUNE_MIN_CYCLES);
            
            snprintf(_state.autoTuneStatus, sizeof(_state.autoTuneStatus),
                     "Ciclo %d/%d", _cycleCount, AUTOTUNE_MIN_CYCLES);
        }
    }

    void calculatePID() {
        _running = false;

        // Average period and amplitude
        float avgPeriod = _periodSum / _cycleCount;  // Tu (ultimate period) in seconds
        float avgAmplitude = _amplitudeSum / _cycleCount;

        // Calculate ultimate gain Ku
        // For relay feedback: Ku = (4 * d) / (pi * a)
        // where d = relay output amplitude, a = oscillation amplitude
        float d = AUTOTUNE_STEP_OUTPUT;
        float Ku = (4.0f * d) / (PI * avgAmplitude);

        // Ziegler-Nichols PID tuning rules
        // Kp = 0.6 * Ku
        // Ki = 1.2 * Ku / Tu  (or Ti = Tu / 2, Ki = Kp / Ti)
        // Kd = 0.075 * Ku * Tu (or Td = Tu / 8, Kd = Kp * Td)
        
        float Kp = 0.6f * Ku;
        float Ki = (1.2f * Ku) / avgPeriod;
        float Kd = 0.075f * Ku * avgPeriod;

        // Clamp to reasonable values
        Kp = constrain(Kp, 1.0f, PID_KP_MAX);
        Ki = constrain(Ki, 0.0001f, PID_KI_MAX);
        Kd = constrain(Kd, 0.0f, PID_KD_MAX);

        DEBUG_PRINTF("[AutoTune] Results: Tu=%.2fs, Ku=%.2f\n", avgPeriod, Ku);
        DEBUG_PRINTF("[AutoTune] PID: Kp=%.2f Ki=%.4f Kd=%.1f\n", Kp, Ki, Kd);

        // Apply to PID controller and save
        _pid.setTunings(Kp, Ki, Kd, true);

        // Update state
        _state.autoTuneActive = false;
        _state.autoTuneProgress = 100;
        setStr(_state.autoTuneStatus, "Concluido!");
        _state.mode = OperatingMode::Idle;

        // Turn off heater
        bus().publish(EventType::HeaterStateChanged, false);

        // Publish completion event with results
        char resultJson[128];
        snprintf(resultJson, sizeof(resultJson), 
                "{\"kp\":%.2f,\"ki\":%.4f,\"kd\":%.1f,\"tu\":%.2f,\"ku\":%.2f}",
                Kp, Ki, Kd, avgPeriod, Ku);
        bus().publish(EventType::AutoTuneCompleted, String(resultJson));

        sendResult(Kp, Ki, Kd);
    }

    void fail(const char* reason) {
        _running = false;
        _state.autoTuneActive = false;
        _state.autoTuneProgress = 0;
        snprintf(_state.autoTuneStatus, sizeof(_state.autoTuneStatus), "Erro: %s", reason);
        _state.mode = OperatingMode::Idle;

        // Turn off heater
        bus().publish(EventType::HeaterStateChanged, false);

        DEBUG_PRINTF("[AutoTune] Failed: %s\n", reason);
        bus().publish(EventType::AutoTuneFailed, String(reason));
        sendProgress();
    }

    void sendProgress() {
        _doc.clear();
        _doc[Protocol::FIELD_TYPE] = Protocol::EVT_AUTOTUNE_STATUS;
        _doc["active"] = _state.autoTuneActive;
        _doc["progress"] = _state.autoTuneProgress;
        _doc["status"] = (const char*)_state.autoTuneStatus;
        _doc["cycles"] = _cycleCount;
        _doc["setpoint"] = _setpoint;

        _jsonBuf = "";
        serializeJson(_doc, _jsonBuf);
        bus().publish(EventType::BLESend, _jsonBuf);
    }

    void sendResult(float kp, float ki, float kd) {
        _doc.clear();
        _doc[Protocol::FIELD_TYPE] = Protocol::EVT_AUTOTUNE_RESULT;
        _doc["kp"] = kp;
        _doc["ki"] = ki;
        _doc["kd"] = kd;
        _doc["success"] = true;

        _jsonBuf = "";
        serializeJson(_doc, _jsonBuf);
        bus().publish(EventType::BLESend, _jsonBuf);
    }

    // Reused doc + buffer — sendProgress/sendResult fire each cycle and
    // previously allocated both per call.
    JsonDocument _doc;
    String       _jsonBuf;
};
