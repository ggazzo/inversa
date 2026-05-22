#pragma once

// Pure-logic core of the Thermal Watchdog (001-thermal-watchdog).
//
// All trip-condition math lives here, free of Arduino includes, GPIO,
// NVS, EventBus or FreeRTOS. ThermalWatchdogPlugin composes this header
// with the IO glue; tests exercise it directly under `env:native`.
//
// Inputs are pushed in by the plugin every WATCHDOG_SAMPLE_INTERVAL_MS
// (5 Hz). Outputs are boolean "should trip" decisions and one cause hint.

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <array>

namespace watchdog {

// ─── Sensor fault timer (RN-02) ─────────────────────────────
// Accumulates contiguous time with tempSensorOk == false. Resets to 0
// the moment sensor becomes OK again. Trips when contiguous time
// exceeds the configured threshold.
class SensorFaultTimer {
public:
    void tick(bool sensorOk, uint32_t deltaMs) {
        _faultMs = sensorOk ? 0u : (_faultMs + deltaMs);
    }
    bool shouldTrip(uint32_t thresholdMs) const { return _faultMs >= thresholdMs; }
    uint32_t faultMs() const { return _faultMs; }
    void reset() { _faultMs = 0; }
private:
    uint32_t _faultMs = 0;
};

// ─── Loop-stuck heartbeat (RN-03) ───────────────────────────
// Increments on every kick() (called by main loop). The ISR samples
// this counter every WATCHDOG_ISR_INTERVAL_US and trips when no
// increment has been observed for thresholdMs.
class LoopHeartbeat {
public:
    void kick(uint32_t nowMs) { _lastKickMs = nowMs; }
    uint32_t msSinceKick(uint32_t nowMs) const { return nowMs - _lastKickMs; }
    bool shouldTrip(uint32_t nowMs, uint32_t thresholdMs) const {
        return msSinceKick(nowMs) >= thresholdMs;
    }
    uint32_t lastKickMs() const { return _lastKickMs; }
private:
    uint32_t _lastKickMs = 0;
};

// ─── Adaptive gradient detector (RN-04) ─────────────────────
// Stores the last N absolute differences |T_i - T_{i-1}|. Detects
// outliers via running median × factor. Returns false until the window
// is full (pre-warm period — avoids false positives at startup).
//
// Template parameter MAX_WINDOW caps RAM at compile time. Effective
// window length is set at runtime via configure() but must be ≤ MAX_WINDOW.
template <size_t MAX_WINDOW>
class GradientDetector {
public:
    void configure(uint8_t window, uint8_t factor, float floorC) {
        _window = (window > MAX_WINDOW) ? MAX_WINDOW : window;
        _factor = factor;
        _floorC = floorC;
        reset();
    }

    void reset() {
        _head = 0;
        _filled = 0;
        _lastTemp = 0.0f;
        _hasLast = false;
    }

    // Push a fresh temperature sample. Returns true if this sample
    // should trip the watchdog (gradient outlier on a full window).
    bool addSample(float temp) {
        if (!_hasLast) {
            _lastTemp = temp;
            _hasLast = true;
            return false;
        }
        // Skip repeated reads of the same filtered value. The watchdog
        // polls at 5 Hz but TemperaturePlugin only refreshes gState at
        // ~1 Hz, so without this guard 4 out of every 5 samples land in
        // the buffer with dT = 0, dragging the median down to zero and
        // collapsing the threshold onto the floor on every real update.
        if (temp == _lastTemp) {
            return false;
        }
        float dT = temp - _lastTemp;
        _lastTemp = temp;
        float absDt = dT < 0 ? -dT : dT;

        // Decide before inserting, so the outlier itself does not skew
        // the median it is being compared against.
        bool trip = false;
        if (_filled >= _window && _window > 0) {
            float median = computeMedian();
            // Floor guards against zero/near-zero median (steady plant):
            // a stable system has tiny dT noise, but the multiplier still
            // needs an absolute lower bound so the first real transient
            // doesn't trip. Floor is tunable via NVS (wd_cfg_grad_floor).
            float threshold = static_cast<float>(_factor) * median;
            if (threshold < _floorC) threshold = _floorC;
            if (absDt > threshold) trip = true;
        }

        _buf[_head] = absDt;
        _head = (_head + 1) % _window;
        if (_filled < _window) ++_filled;
        return trip;
    }

    bool windowFull() const { return _filled >= _window; }
    size_t filledCount() const { return _filled; }

private:
    float computeMedian() const {
        // Copy active region, sort, return middle. _filled ≤ _window ≤ MAX_WINDOW.
        std::array<float, MAX_WINDOW> tmp{};
        for (size_t i = 0; i < _filled; ++i) tmp[i] = _buf[i];
        std::sort(tmp.begin(), tmp.begin() + _filled);
        return (_filled & 1u) ? tmp[_filled / 2]
                              : 0.5f * (tmp[_filled / 2 - 1] + tmp[_filled / 2]);
    }

    std::array<float, MAX_WINDOW> _buf{};
    size_t  _head    = 0;
    size_t  _filled  = 0;
    uint8_t _window  = 0;
    uint8_t _factor  = 5;
    float   _floorC  = 0.25f;
    float   _lastTemp = 0.0f;
    bool    _hasLast = false;
};

// ─── Auto-reset cooldown timer (RN-05) ──────────────────────
// Accumulates contiguous time with currentTemp below the safe threshold
// AND with no other trip condition active. Any violation resets the
// counter. Becomes "ready" when contiguous time reaches coolMinMs.
class CooldownTimer {
public:
    void tick(float currentTemp, float safeC, bool allOtherConditionsOk, uint32_t deltaMs) {
        if (allOtherConditionsOk && currentTemp < safeC) {
            _accumMs += deltaMs;
        } else {
            _accumMs = 0;
        }
    }
    bool ready(uint32_t coolMinMs) const { return _accumMs >= coolMinMs; }
    uint32_t accumMs() const { return _accumMs; }
    void reset() { _accumMs = 0; }
private:
    uint32_t _accumMs = 0;
};

// ─── Overtemp + reset-margin helpers ────────────────────────
// Pure functions, no state.
inline bool isOvertemp(float currentTemp, float hardStopC) {
    return currentTemp > hardStopC;
}

// RN-08: reset rejected if currentTemp >= hardStop - margin.
inline bool canResetLatch(float currentTemp, float hardStopC, float resetMarginC) {
    return currentTemp < (hardStopC - resetMarginC);
}

}  // namespace watchdog
