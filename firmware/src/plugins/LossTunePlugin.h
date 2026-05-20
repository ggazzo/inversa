#pragma once

// LossTunePlugin — auto-tune the pot's heat-transfer coefficient h
// (W/m²·K) by observing Newton-law cooling and fitting τ.
//
// Physics:
//   With heater off: m·c·dT/dt = -h·A·(T - T_amb)
//   Solution:        T(t) = T_amb + ΔT0 · exp(-t/τ),  τ = m·c / (h·A)
//   Linearize:       y(t) = ln(T(t) - T_amb) = ln(ΔT0) - t/τ
//   Least-squares slope of y vs t → τ = -1/slope, then h = (m·c) / (τ·A).
//
// State machine: IDLE → PREFLIGHT → HEAT → SOAK → DECAY → FIT → RESULT
//   (RESULT awaits explicit user accept/reject; cancel from any phase → IDLE).
//
// The plugin runs the fit on-device (single-pass Welford on y vs t) so the
// official result is authoritative. Decay samples are also streamed to BLE
// in 5 s batches so the UI can overlay the fit on the live curve.
//
// Safety: this plugin does NOT disarm the ThermalWatchdog. HEAT target is
// capped well below WATCHDOG_DEFAULT_HARDSTOP_C. The PID drives the heater
// during HEAT/SOAK; during DECAY the plugin asks HeaterPlugin to release
// the SSR by publishing HeaterStateChanged=false and dropping setpoint.

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cmath>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/NVSStorage.h"
#include "../core/ThermalCalc.h"
#include "../core/constants.h"
#include "../models/MachineState.h"
#include "../protocol/protocol.h"

class LossTunePlugin : public Plugin {
public:
    const char* getName() const override { return "LossTune"; }

    bool setup() override {
        gState.lossTuneActive      = false;
        gState.lossTunePhase       = LossTunePhase::IDLE;
        gState.lossTuneProgressPct = 0;
        gState.lossTuneR2          = 0.0f;
        gState.lossTuneFittedCoeff = 0.0f;
        gState.lossTuneSampleCount = 0;
        setStr(gState.lossTuneError, "");
        return true;
    }

    void loop() override {
        if (gState.lossTunePhase == LossTunePhase::IDLE
            || gState.lossTunePhase == LossTunePhase::RESULT
            || gState.lossTunePhase == LossTunePhase::ERROR) {
            return;
        }

        uint32_t now = millis();
        switch (gState.lossTunePhase) {
            case LossTunePhase::PREFLIGHT: tickPreflight(now); break;
            case LossTunePhase::HEAT:      tickHeat(now);      break;
            case LossTunePhase::SOAK:      tickSoak(now);      break;
            case LossTunePhase::DECAY:     tickDecay(now);     break;
            case LossTunePhase::FIT:       tickFit();          break;
            default: break;
        }
    }

    // ── Public API (called by CommandHandler) ───────────────

    // Begin a tune for the given lid mode. Returns nullptr on success,
    // else a static error string suitable for the BLE error response.
    const char* start(LidState targetMode) {
        if (gState.lossTuneActive) return "already_running";
        if (gState.mode == OperatingMode::Recipe
            || gState.mode == OperatingMode::Tuning) {
            return "busy_mode";
        }
        if (gState.watchdogTripped)      return "watchdog_latched";
        if (!gState.tempSensorOk)        return "sensor_fault";
        if (gState.volumeLiters   <= 0.0f) return "volume_invalid";
        if (gState.vesselDiameter <= 0.0f) return "diameter_invalid";

        // Capture ambient at start. Use whatever getEffectiveAmbient
        // returns now; record the source used so the UI can warn the
        // user when the fit was based on a manual (unverifiable) value.
        gState.lossTuneAmbientStart = getEffectiveAmbient();
        gState.lossTuneAmbientSrc   = gState.ambientSource;
        gState.lossTuneTargetMode   = targetMode;

        // Pre-flight passed.
        resetSampler();
        gState.lossTuneActive       = true;
        gState.lossTunePhase        = LossTunePhase::PREFLIGHT;
        gState.lossTuneProgressPct  = 0;
        gState.lossTuneR2           = 0.0f;
        gState.lossTuneFittedCoeff  = 0.0f;
        gState.lossTuneSampleCount  = 0;
        setStr(gState.lossTuneError, "");
        gState.mode                 = OperatingMode::Tuning;
        _phaseStartMs               = millis();

        DEBUG_PRINTF("[LossTune] Start mode=%s T_amb=%.2f src=%u\n",
                     targetMode == LidState::ON ? "lidOn" : "lidOff",
                     gState.lossTuneAmbientStart,
                     (unsigned)gState.lossTuneAmbientSrc);
        publishStatus();
        return nullptr;
    }

    void cancel(const char* reason = "user_cancel") {
        if (!gState.lossTuneActive) return;
        releaseHeater();
        setError(reason);
        DEBUG_PRINTF("[LossTune] Cancelled: %s\n", reason);
    }

    // Persist fitted coeff into the targeted slot. Only valid in RESULT.
    bool accept() {
        if (gState.lossTunePhase != LossTunePhase::RESULT) return false;
        float coeff = gState.lossTuneFittedCoeff;
        LidState mode = gState.lossTuneTargetMode;
        if (mode == LidState::ON)  gState.heatLossCoeffLidOn  = coeff;
        else                       gState.heatLossCoeffLidOff = coeff;
        NVSStorage::instance().saveThermalLossCoeff((uint8_t)mode, coeff);

        gState.lossTuneActive = false;
        gState.lossTunePhase  = LossTunePhase::IDLE;
        gState.mode           = OperatingMode::Idle;
        publishStatus();
        DEBUG_PRINTF("[LossTune] Accepted: mode=%s h=%.2f\n",
                     mode == LidState::ON ? "lidOn" : "lidOff", coeff);
        return true;
    }

    bool reject() {
        if (gState.lossTunePhase != LossTunePhase::RESULT) return false;
        gState.lossTuneActive = false;
        gState.lossTunePhase  = LossTunePhase::IDLE;
        gState.mode           = OperatingMode::Idle;
        publishStatus();
        DEBUG_PRINTLN("[LossTune] Rejected");
        return true;
    }

private:
    // ── Phase ticks ─────────────────────────────────────────

    void tickPreflight(uint32_t now) {
        // Compute heat target: amb + delta, capped at LOSSTUNE_TARGET_MAX_C
        // and at watchdog hard-stop minus 10 °C for safety margin.
        float ambient  = getEffectiveAmbient();
        float capWdog  = gState.watchdogHardStopC - 10.0f;
        float capHard  = LOSSTUNE_TARGET_MAX_C;
        _heatTarget    = std::min({ambient + LOSSTUNE_TARGET_DELTA_C, capWdog, capHard});
        if (_heatTarget <= ambient + 10.0f) {
            setError("delta_too_small");
            return;
        }
        gState.targetTemp = _heatTarget;
        bus().publish(EventType::SetpointChanged, _heatTarget);
        enterPhase(LossTunePhase::HEAT, now);
        DEBUG_PRINTF("[LossTune] PREFLIGHT done, heat target=%.2f°C\n", _heatTarget);
    }

    void tickHeat(uint32_t now) {
        float t = gState.currentTemp;
        if (!gState.tempSensorOk) { cancel("sensor_fault"); return; }
        if (gState.watchdogTripped) { cancel("watchdog_trip"); return; }

        // Reached target band?
        if (t >= _heatTarget - LOSSTUNE_HEAT_SETTLE_BAND_C) {
            enterPhase(LossTunePhase::SOAK, now);
            DEBUG_PRINTLN("[LossTune] HEAT done, entering SOAK");
        }
        updateProgress(/*phaseFrac=*/0.0f, /*phaseSpan=*/0.30f, now);
        publishStatusThrottled(now);
    }

    void tickSoak(uint32_t now) {
        float t = gState.currentTemp;
        if (!gState.tempSensorOk) { cancel("sensor_fault"); return; }
        if (gState.watchdogTripped) { cancel("watchdog_trip"); return; }

        if (fabsf(t - _heatTarget) > LOSSTUNE_HEAT_SETTLE_BAND_C * 2.0f) {
            // Drifted out of band during soak — let PID pull it back, restart soak.
            _phaseStartMs = now;
        } else if ((now - _phaseStartMs) >= LOSSTUNE_SOAK_MS) {
            // Soak complete → release heater, begin decay.
            releaseHeater();
            _decayT0    = gState.currentTemp;
            _decayStart = now;
            resetSampler();
            enterPhase(LossTunePhase::DECAY, now);
            DEBUG_PRINTF("[LossTune] SOAK done, T0=%.2f°C, releasing heater\n", _decayT0);
        }
        updateProgress(0.30f, 0.05f, now);
        publishStatusThrottled(now);
    }

    void tickDecay(uint32_t now) {
        if (gState.watchdogTripped) { cancel("watchdog_trip"); return; }
        if (!gState.tempSensorOk)   { cancel("sensor_fault"); return; }

        // Belt-and-suspenders: re-assert heater off each tick.
        if (gState.heaterOn) releaseHeater();

        // Sample at 1 Hz.
        if ((now - _lastSampleMs) < LOSSTUNE_SAMPLE_INTERVAL_MS) return;
        _lastSampleMs = now;

        float t   = gState.currentTemp;
        float amb = getEffectiveAmbient();
        float deltaT = t - amb;
        if (deltaT <= 0.5f) {
            // Below noise floor — stop decay early, attempt fit if we have data.
            enterPhase(LossTunePhase::FIT, now);
            return;
        }

        uint32_t tSec = (now - _decayStart) / 1000;
        if (gState.lossTuneSampleCount < LOSSTUNE_BUFFER_SIZE) {
            _sampleT[gState.lossTuneSampleCount] = (uint16_t)tSec;
            _sampleC[gState.lossTuneSampleCount] = t;
            gState.lossTuneSampleCount++;
        }

        // Emit batched samples to UI every N samples.
        if ((gState.lossTuneSampleCount % LOSSTUNE_SAMPLE_EMIT_BATCH) == 0) {
            publishSampleBatch();
        }

        // Stop conditions.
        bool nearAmbient = (deltaT <= LOSSTUNE_STOP_DELTA_C);
        bool timeout     = ((now - _decayStart) >= LOSSTUNE_MAX_MS);
        if (nearAmbient || timeout) {
            gState.lossTuneAmbientEnd = amb;
            enterPhase(LossTunePhase::FIT, now);
            DEBUG_PRINTF("[LossTune] DECAY done (samples=%u reason=%s)\n",
                         (unsigned)gState.lossTuneSampleCount,
                         nearAmbient ? "near_ambient" : "timeout");
        }

        updateProgress(0.35f, 0.60f, now,
                       (float)(now - _decayStart) / (float)LOSSTUNE_MAX_MS);
        publishStatusThrottled(now);
    }

    void tickFit() {
        // Drop head (mixing transient) and tail (deltaT below noise floor).
        float amb = (gState.lossTuneAmbientEnd != 0.0f)
                    ? 0.5f * (gState.lossTuneAmbientStart + gState.lossTuneAmbientEnd)
                    : gState.lossTuneAmbientStart;

        const uint16_t headSec = LOSSTUNE_DROP_HEAD_S;
        // Single-pass LSQ on (t_i, y_i) where y_i = ln(T_i - amb).
        double sumX  = 0, sumY = 0, sumXX = 0, sumXY = 0, sumYY = 0;
        uint32_t n = 0;
        for (uint16_t i = 0; i < gState.lossTuneSampleCount; i++) {
            uint16_t tSec = _sampleT[i];
            if (tSec < headSec) continue;
            float dT = _sampleC[i] - amb;
            if (dT <= 1.0f) continue;        // noise / below floor
            double x = (double)tSec;
            double y = (double)logf(dT);
            sumX  += x;
            sumY  += y;
            sumXX += x * x;
            sumXY += x * y;
            sumYY += y * y;
            n++;
        }

        if (n < LOSSTUNE_MIN_SAMPLES) { setError("samples_low"); return; }

        double meanX = sumX / n;
        double meanY = sumY / n;
        double varX  = sumXX - (sumX * sumX) / n;
        double covXY = sumXY - (sumX * sumY) / n;
        if (varX <= 0.0) { setError("fit_singular"); return; }
        double slope = covXY / varX;
        // R² = (covXY)^2 / (varX · varY)
        double varY  = sumYY - (sumY * sumY) / n;
        if (varY <= 0.0) { setError("fit_flat"); return; }
        double r2 = (covXY * covXY) / (varX * varY);

        if (slope >= 0.0) { setError("fit_positive_slope"); return; }
        double tau = -1.0 / slope;

        if (tau < LOSSTUNE_TAU_MIN_S || tau > LOSSTUNE_TAU_MAX_S) {
            setError("tau_out_of_range");
            return;
        }
        if (r2 < LOSSTUNE_MIN_R2) { setError("r2_low"); return; }

        // Compute h = (m·c) / (τ·A) where m = volumeL · 1 kg/L, c = 4186 J/kg·K.
        float surfaceA = ThermalCalc::cylinderSurfaceArea(
            gState.volumeLiters, gState.vesselDiameter);
        float massKg   = gState.volumeLiters * ThermalCalc::WATER_DENSITY;
        float coeff    = (massKg * ThermalCalc::SPECIFIC_HEAT_WATER)
                         / ((float)tau * surfaceA);

        if (coeff < LOSSTUNE_COEFF_MIN || coeff > LOSSTUNE_COEFF_MAX) {
            setError("coeff_out_of_range");
            return;
        }

        // Ambient drift gate only for SENSOR mode (manual is unverifiable).
        if (gState.lossTuneAmbientSrc == AmbientSource::SENSOR) {
            float drift = fabsf(gState.lossTuneAmbientEnd
                                - gState.lossTuneAmbientStart);
            if (drift > LOSSTUNE_AMBIENT_DRIFT_C) {
                setError("ambient_drift");
                return;
            }
        }

        gState.lossTuneFittedCoeff = coeff;
        gState.lossTuneR2          = (float)r2;
        gState.lossTunePhase       = LossTunePhase::RESULT;
        gState.lossTuneProgressPct = 100;
        publishResult(tau, n);
        DEBUG_PRINTF("[LossTune] FIT done: τ=%.1fs h=%.2f R²=%.4f n=%u\n",
                     tau, coeff, (float)r2, (unsigned)n);
    }

    // ── Helpers ─────────────────────────────────────────────

    void enterPhase(LossTunePhase p, uint32_t now) {
        gState.lossTunePhase = p;
        _phaseStartMs        = now;
        publishStatus();
    }

    void releaseHeater() {
        gState.targetTemp = 0.0f;
        bus().publish(EventType::SetpointChanged, 0.0f);
        bus().publish(EventType::HeaterStateChanged, false);
    }

    void resetSampler() {
        _lastSampleMs = 0;
        gState.lossTuneSampleCount = 0;
        _emittedThrough = 0;
    }

    void setError(const char* reason) {
        releaseHeater();
        setStr(gState.lossTuneError, reason);
        gState.lossTunePhase = LossTunePhase::ERROR;
        gState.lossTuneActive = false;
        gState.mode = OperatingMode::Idle;
        publishStatus();
    }

    void updateProgress(float baseFrac, float spanFrac, uint32_t /*now*/,
                        float subFrac = 0.0f) {
        float frac = baseFrac + spanFrac * std::clamp(subFrac, 0.0f, 1.0f);
        gState.lossTuneProgressPct = (uint8_t)std::clamp(frac * 100.0f, 0.0f, 100.0f);
    }

    void publishStatusThrottled(uint32_t now) {
        if ((now - _lastStatusMs) < 1000) return;
        _lastStatusMs = now;
        publishStatus();
    }

    void publishStatus() {
        JsonDocument doc;
        doc[Protocol::FIELD_TYPE] = Protocol::EVT_LOSSTUNE_STATUS;
        doc["ph"]    = getLossTunePhaseName(gState.lossTunePhase);
        doc["pct"]   = gState.lossTuneProgressPct;
        doc["mode"]  = gState.lossTuneTargetMode == LidState::ON ? "lidOn" : "lidOff";
        doc["amb0"]  = gState.lossTuneAmbientStart;
        doc["ambE"]  = gState.lossTuneAmbientEnd;
        doc["src"]   = gState.lossTuneAmbientSrc == AmbientSource::SENSOR ? "sensor" : "manual";
        doc["n"]     = gState.lossTuneSampleCount;
        doc["err"]   = gState.lossTuneError;
        String s;
        serializeJson(doc, s);
        bus().publish(EventType::BLESend, s);
    }

    void publishSampleBatch() {
        if (_emittedThrough >= gState.lossTuneSampleCount) return;
        JsonDocument doc;
        doc[Protocol::FIELD_TYPE] = Protocol::EVT_LOSSTUNE_SAMPLE;
        doc["t0"] = _sampleT[_emittedThrough];
        JsonArray arr = doc["T"].to<JsonArray>();
        for (uint16_t i = _emittedThrough; i < gState.lossTuneSampleCount; i++) {
            arr.add(_sampleC[i]);
        }
        _emittedThrough = gState.lossTuneSampleCount;
        String s;
        serializeJson(doc, s);
        bus().publish(EventType::BLESend, s);
    }

    void publishResult(double tau, uint32_t n) {
        JsonDocument doc;
        doc[Protocol::FIELD_TYPE] = Protocol::EVT_LOSSTUNE_RESULT;
        doc["mode"] = gState.lossTuneTargetMode == LidState::ON ? "lidOn" : "lidOff";
        doc["h"]    = gState.lossTuneFittedCoeff;
        doc["tau"]  = tau;
        doc["r2"]   = gState.lossTuneR2;
        doc["n"]    = n;
        doc["amb0"] = gState.lossTuneAmbientStart;
        doc["ambE"] = gState.lossTuneAmbientEnd;
        doc["src"]  = gState.lossTuneAmbientSrc == AmbientSource::SENSOR ? "sensor" : "manual";
        String s;
        serializeJson(doc, s);
        bus().publish(EventType::BLESend, s);
        publishStatus();
    }

    // ── State ───────────────────────────────────────────────
    uint32_t _phaseStartMs   = 0;
    uint32_t _decayStart     = 0;
    uint32_t _lastSampleMs   = 0;
    uint32_t _lastStatusMs   = 0;
    uint16_t _emittedThrough = 0;
    float    _decayT0        = 0.0f;
    float    _heatTarget     = 0.0f;

    uint16_t _sampleT[LOSSTUNE_BUFFER_SIZE] = {0};
    float    _sampleC[LOSSTUNE_BUFFER_SIZE] = {0};
};
