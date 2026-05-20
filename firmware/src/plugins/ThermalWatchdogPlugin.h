#pragma once

// ThermalWatchdogPlugin — independent safety layer that forces the SSR
// off when any of four conditions trip (001-thermal-watchdog).
//
// Detection paths (RN-01 to RN-04):
//   - Overtemp:    currentTemp > T_HARDSTOP
//   - SensorFault: tempSensorOk == false for ≥ T_SENSOR_FAULT_MS
//   - LoopStuck:   main loop has not called kick() for ≥ T_LOOP_STUCK_MS
//                  (checked from an esp_timer task at 10 Hz)
//   - Gradient:    |dT| > GRAD_FACTOR × median(last GRAD_WINDOW deltas)
//
// On trip the SSR GPIO is forced LOW via gpio_set_level() (bypassing the
// EventBus to avoid DT-24 iterator-invalidation risk), gState flags are
// updated, NVS persists the latched state + trip stats, and a BLE event
// is published. The latch survives reboot until either:
//   - the user sends req:watchdog:reset under safe conditions (RN-08), or
//   - cooldown auto-reset kicks in (currentTemp < T_SAFE for T_COOL_MIN_MS
//     contiguous, only when auto_reset_enabled == true; RN-05).
//
// Pure logic lives in plugins/watchdog/WatchdogEvaluator.h and is tested
// natively in firmware/test/test_watchdog/. This plugin is the IO glue.

#include <Arduino.h>
#include <ArduinoJson.h>
#ifndef NATIVE_BUILD
#include <esp_timer.h>
#include <driver/gpio.h>
#endif
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/constants.h"
#include "../core/NVSStorage.h"
#include "../models/MachineState.h"
#include "../protocol/protocol.h"
#include "IExternalCut.h"
#include "watchdog/WatchdogEvaluator.h"
#ifdef HIL_BUILD
#include "../hil/HilState.h"
#endif

#ifdef NATIVE_BUILD
// The simulator does not link against ESP-IDF — stub the few symbols the
// plugin touches so the class still compiles. Loop-stuck detection via
// hardware timer is meaningless in the simulator anyway (no SSR pin to
// cut). The three other trip paths (overtemp, sensor fault, gradient)
// still exercise their pure logic.
using esp_timer_handle_t = void*;
static inline void gpio_set_level(int /*pin*/, int /*level*/) {}
using gpio_num_t = int;
#endif

class ThermalWatchdogPlugin : public Plugin {
public:
    const char* getName() const override { return "Watchdog"; }

    // ── Configuration (called by CommandHandler) ────────────
    struct Config {
        float    hardStopC;
        uint32_t sensorFaultMs;
        uint32_t loopStuckMs;
        uint8_t  gradFactor;
        uint8_t  gradWindow;
        float    safeAutoresetC;
        uint32_t coolMinMs;
        bool     autoResetEnabled;
    };

    static bool validateConfig(const Config& c, const char** badField = nullptr) {
        auto fail = [&](const char* f) { if (badField) *badField = f; return false; };
        if (c.hardStopC       < WATCHDOG_HARDSTOP_MIN_C       || c.hardStopC       > WATCHDOG_HARDSTOP_MAX_C)       return fail("hard_stop");
        if (c.sensorFaultMs   < WATCHDOG_SENSOR_FAULT_MIN_MS  || c.sensorFaultMs   > WATCHDOG_SENSOR_FAULT_MAX_MS)  return fail("sensor_fault_ms");
        if (c.loopStuckMs     < WATCHDOG_LOOP_STUCK_MIN_MS    || c.loopStuckMs     > WATCHDOG_LOOP_STUCK_MAX_MS)    return fail("loop_stuck_ms");
        if (c.gradFactor      < WATCHDOG_GRAD_FACTOR_MIN      || c.gradFactor      > WATCHDOG_GRAD_FACTOR_MAX)      return fail("grad_factor");
        if (c.gradWindow      < WATCHDOG_GRAD_WINDOW_MIN      || c.gradWindow      > WATCHDOG_GRAD_WINDOW_MAX)      return fail("grad_window");
        if (c.safeAutoresetC  < WATCHDOG_SAFE_AUTORESET_MIN_C || c.safeAutoresetC  > WATCHDOG_SAFE_AUTORESET_MAX_C) return fail("safe_autoreset_c");
        if (c.coolMinMs       < WATCHDOG_COOL_MIN_MIN_MS      || c.coolMinMs       > WATCHDOG_COOL_MIN_MAX_MS)      return fail("cool_min_ms");
        return true;
    }

    // Applies, persists, reconfigures detector. Caller validates first.
    void setConfig(const Config& c) {
        gState.watchdogHardStopC        = c.hardStopC;
        gState.watchdogSensorFaultMs    = c.sensorFaultMs;
        gState.watchdogLoopStuckMs      = c.loopStuckMs;
        gState.watchdogGradFactor       = c.gradFactor;
        gState.watchdogGradWindow       = c.gradWindow;
        gState.watchdogSafeAutoresetC   = c.safeAutoresetC;
        gState.watchdogCoolMinMs        = c.coolMinMs;
        gState.watchdogAutoResetEnabled = c.autoResetEnabled;

        NVSStorage::instance().saveWatchdogConfig(
            c.hardStopC, c.sensorFaultMs, c.loopStuckMs, c.gradFactor,
            c.gradWindow, c.safeAutoresetC, c.coolMinMs, c.autoResetEnabled);

        _gradient.configure(c.gradWindow, c.gradFactor);
        bus().publish(EventType::WatchdogConfigChanged);
    }

    Config getConfig() const {
        return Config{
            gState.watchdogHardStopC, gState.watchdogSensorFaultMs,
            gState.watchdogLoopStuckMs, gState.watchdogGradFactor,
            gState.watchdogGradWindow, gState.watchdogSafeAutoresetC,
            gState.watchdogCoolMinMs, gState.watchdogAutoResetEnabled
        };
    }

    // ── External cut hook (RF-10) ───────────────────────────
    void setExternalCut(IExternalCut* cut) { _externalCut = cut; }

    // ── Public commands (from CommandHandler) ───────────────
    enum class ResetResult { Accepted, NotTripped, StillUnsafe };

    ResetResult requestReset() {
        if (!gState.watchdogTripped) return ResetResult::NotTripped;
        if (!watchdog::canResetLatch(gState.currentTemp,
                                     gState.watchdogHardStopC,
                                     WATCHDOG_RESET_MARGIN_C)) {
            return ResetResult::StillUnsafe;
        }
        clearLatch(/*autoReset=*/false);
        return ResetResult::Accepted;
    }

    // Heartbeat from main.cpp loop().
    void kick() {
        uint32_t now = millis();
        _heartbeat.kick(now);
        _heartbeatMs = now;  // for the esp_timer callback (volatile-ish)
    }

#ifdef HIL_BUILD
    // HIL-only: force a trip from the harness. The cause maps 1:1 to
    // WatchdogCause. Goes through trip() so all side effects (GPIO, NVS,
    // events, BLE) fire identically to a natural trip. This lets the host
    // bridge exercise the same code path that runs in production without
    // having to manipulate sensor inputs precisely.
    void hilForceTrip(WatchdogCause cause) { trip(cause); }

    // HIL-only: suspend automatic detection. When disabled, neither the
    // sampling loop nor the esp_timer ISR will trip on overtemp, sensor
    // fault, gradient, or loop-stuck conditions. `force watchdog.trip`
    // still works — the harness can always invoke an explicit trip.
    //
    // Why this exists: virtual-clock advances (`clock advance N`) and
    // synthetic NTC steps (`set ntc.c X`) routinely violate the gradient
    // and loop-stuck heuristics, even though the system is fine. Cenarios
    // that want to script time-warps without those false positives turn
    // detection off; cenarios that want to assert the watchdog's natural
    // behavior leave it on (the default).
    void hilSetDetections(bool on) { _detectionsEnabled = on; }
    bool hilDetectionsEnabled() const { return _detectionsEnabled; }

    // HIL-only: bump the heartbeat marker by `deltaMs` to absorb a
    // virtual-clock advance. The main loop will catch up on its next
    // iteration, but the ISR runs at real cadence and would otherwise see
    // an artificial gap. Safe to call even with detections suspended.
    void hilAbsorbClockAdvance(uint32_t deltaMs) {
        _heartbeatMs += deltaMs;
        _lastSampleMs += deltaMs;
    }
#endif

    // ── Plugin interface ────────────────────────────────────
    bool setup() override {
        // 1. Load NVS config (defaults applied if absent)
        auto& nvs = NVSStorage::instance();
        Config cfg{
            nvs.loadWatchdogHardStopC      (WATCHDOG_DEFAULT_HARDSTOP_C),
            nvs.loadWatchdogSensorFaultMs  (WATCHDOG_DEFAULT_SENSOR_FAULT_MS),
            nvs.loadWatchdogLoopStuckMs    (WATCHDOG_DEFAULT_LOOP_STUCK_MS),
            nvs.loadWatchdogGradFactor     (WATCHDOG_DEFAULT_GRAD_FACTOR),
            nvs.loadWatchdogGradWindow     (WATCHDOG_DEFAULT_GRAD_WINDOW),
            nvs.loadWatchdogSafeAutoresetC (WATCHDOG_DEFAULT_SAFE_AUTORESET_C),
            nvs.loadWatchdogCoolMinMs      (WATCHDOG_DEFAULT_COOL_MIN_MS),
            nvs.loadWatchdogAutoResetEnabled(true)
        };
        // Mirror into gState so other plugins/UI see it.
        gState.watchdogHardStopC        = cfg.hardStopC;
        gState.watchdogSensorFaultMs    = cfg.sensorFaultMs;
        gState.watchdogLoopStuckMs      = cfg.loopStuckMs;
        gState.watchdogGradFactor       = cfg.gradFactor;
        gState.watchdogGradWindow       = cfg.gradWindow;
        gState.watchdogSafeAutoresetC   = cfg.safeAutoresetC;
        gState.watchdogCoolMinMs        = cfg.coolMinMs;
        gState.watchdogAutoResetEnabled = cfg.autoResetEnabled;
        _gradient.configure(cfg.gradWindow, cfg.gradFactor);

        // 2. Restore trip stats
        gState.watchdogTripCount    = nvs.loadWatchdogTripCount(0);
        gState.watchdogLastCause    = (WatchdogCause)nvs.loadWatchdogLastCause(0);
        gState.watchdogLastTripUnix = nvs.loadWatchdogLastTripUnix(0);

        // 3. Restore latched. If true, hold heater off from boot.
        bool latched = nvs.loadWatchdogLatched(false);
        gState.watchdogTripped = latched;
        gState.watchdogArmed   = true;
        if (latched) {
            // Force SSR LOW immediately — HeaterPlugin's setup() will run
            // separately but this is the safety belt during the gap.
            gpio_set_level((gpio_num_t)PIN_HEATER_SSR, 0);
            DEBUG_PRINTF("[Watchdog] Latched=true on boot — heater inhibited (last_cause=%s)\n",
                         getWatchdogCauseName(gState.watchdogLastCause));
        } else {
            DEBUG_PRINTLN("[Watchdog] Latched=false on boot (clean state)");
        }

        DEBUG_PRINTF("[Watchdog] Config: hs=%.1f sfm=%u lsm=%u gf=%u gw=%u sa=%.1f cm=%u are=%d\n",
                     cfg.hardStopC, cfg.sensorFaultMs, cfg.loopStuckMs,
                     cfg.gradFactor, cfg.gradWindow, cfg.safeAutoresetC,
                     cfg.coolMinMs, (int)cfg.autoResetEnabled);

        // 4. Initial kick + arm the esp_timer for loop-stuck detection.
        _heartbeat.kick(millis());
        _heartbeatMs = millis();
        _instance = this;
#ifndef NATIVE_BUILD
        esp_timer_create_args_t args = {};
        args.callback = &ThermalWatchdogPlugin::onTimerStatic;
        args.arg      = this;
        args.name     = "wd_loop_stuck";
        // Default dispatch (timer task at high priority). Works on both
        // S3 (dual-core) and C3 (single-core); no ISR_ATTR needed.
        if (esp_timer_create(&args, &_isrTimer) == ESP_OK) {
            esp_timer_start_periodic(_isrTimer, WATCHDOG_ISR_INTERVAL_US);
            DEBUG_PRINTF("[Watchdog] esp_timer armed at %lu us\n",
                         (unsigned long)WATCHDOG_ISR_INTERVAL_US);
        } else {
            DEBUG_PRINTLN("[Watchdog] esp_timer_create FAILED — loop-stuck detection inactive");
        }
#else
        // env:sim — loop-stuck detection is a no-op; sim has no real
        // hardware watchdog to satisfy and no SSR to cut.
        (void)_isrTimer;
#endif

        return true;
    }

    void loop() override {
        uint32_t now = millis();

        // ── Drain any ISR-pending trip (loop-stuck) ─────────
        if (_isrTripPending) {
            _isrTripPending = false;
#ifdef HIL_BUILD
            if (_detectionsEnabled) trip(WatchdogCause::LOOP_STUCK);
#else
            trip(WatchdogCause::LOOP_STUCK);
#endif
        }

#ifdef HIL_BUILD
        // HIL: when detections are suspended, skip the sampling loop too.
        // `force` paths still work via hilForceTrip.
        if (!_detectionsEnabled) {
            _lastSampleMs = now;
            return;
        }
#endif

        // ── Sample at 5 Hz ──────────────────────────────────
        if (now - _lastSampleMs < WATCHDOG_SAMPLE_INTERVAL_MS) return;
        uint32_t delta = (_lastSampleMs == 0) ? WATCHDOG_SAMPLE_INTERVAL_MS
                                              : (now - _lastSampleMs);
        _lastSampleMs = now;

        // ── Update detectors ────────────────────────────────
        _sensorFault.tick(gState.tempSensorOk, delta);

        // Gradient only sees valid samples. Reset it on a fault so the
        // baseline does not get poisoned by sentinel/zero readings.
        if (gState.tempSensorOk) {
            // Sample-feed result is consulted below.
            _gradTrippedThisTick = _gradient.addSample(gState.currentTemp);
        } else {
            _gradient.reset();
            _gradTrippedThisTick = false;
        }

        // Cooldown can only accumulate when no condition is unhappy.
        bool othersOk = gState.tempSensorOk
                        && _heartbeat.msSinceKick(now) < gState.watchdogLoopStuckMs
                        && !watchdog::isOvertemp(gState.currentTemp, gState.watchdogHardStopC);
        _cooldown.tick(gState.currentTemp,
                       gState.watchdogSafeAutoresetC,
                       othersOk,
                       delta);

        // ── Evaluate trip conditions (only if not already latched) ──
        if (!gState.watchdogTripped) {
            if (watchdog::isOvertemp(gState.currentTemp, gState.watchdogHardStopC)) {
                trip(WatchdogCause::OVERTEMP);
            } else if (_sensorFault.shouldTrip(gState.watchdogSensorFaultMs)) {
                trip(WatchdogCause::SENSOR_FAULT);
            } else if (_gradTrippedThisTick) {
                trip(WatchdogCause::GRADIENT);
            }
            // LOOP_STUCK is driven by the esp_timer callback, not here.
        } else {
            // ── Auto-reset by cooldown (RN-05) ──────────────
            if (gState.watchdogAutoResetEnabled
                && _cooldown.ready(gState.watchdogCoolMinMs)) {
                clearLatch(/*autoReset=*/true);
            }
        }
    }

private:
    // ── Trip path ───────────────────────────────────────────
    void trip(WatchdogCause cause) {
        // Idempotent: do not double-fire if already tripped on the same cause.
        if (gState.watchdogTripped && gState.watchdogLastCause == cause) return;

        // 1. Force GPIO LOW immediately. This is the canonical safety act —
        //    everything else can fail and the SSR still ends up off.
        gpio_set_level((gpio_num_t)PIN_HEATER_SSR, 0);

        // 2. External cut path (best-effort; can be nullptr).
        if (_externalCut) _externalCut->trip(cause);

        // 3. Update gState.
        WatchdogCause prev = gState.watchdogLastCause;
        gState.watchdogTripped       = true;
        gState.watchdogLastCause     = cause;
        gState.watchdogTripCount    += 1;
        gState.watchdogLastTripUnix  = gState.rtcAvailable ? gState.rtcTimestamp : 0u;

        // 4. Persist.
        auto& nvs = NVSStorage::instance();
        nvs.saveWatchdogTrip(gState.watchdogTripCount,
                             (uint8_t)cause,
                             gState.watchdogLastTripUnix);
        nvs.saveWatchdogLatched(true);

        // 5. Reset cooldown so a fresh trip restarts the auto-reset window.
        _cooldown.reset();

        // 6. Publish events.
        bus().publish(EventType::WatchdogTripped, (int)cause);
        publishBleEvent(cause, prev, /*isReset=*/false, /*autoReset=*/false);

        DEBUG_PRINTF("[Watchdog] TRIPPED cause=%s count=%u temp=%.2f\n",
                     getWatchdogCauseName(cause),
                     gState.watchdogTripCount,
                     gState.currentTemp);
    }

    void clearLatch(bool autoReset) {
        WatchdogCause prev = gState.watchdogLastCause;
        gState.watchdogTripped   = false;
        gState.watchdogLastCause = WatchdogCause::NONE;
        NVSStorage::instance().clearWatchdogLatched();

        if (_externalCut) _externalCut->clear();

        _cooldown.reset();
        _sensorFault.reset();

        bus().publish(EventType::WatchdogReset, autoReset);
        publishBleEvent(prev, prev, /*isReset=*/true, autoReset);

        DEBUG_PRINTF("[Watchdog] RESET %s prev_cause=%s\n",
                     autoReset ? "(auto)" : "(manual)",
                     getWatchdogCauseName(prev));
    }

    void publishBleEvent(WatchdogCause cause, WatchdogCause prevCause,
                         bool isReset, bool autoReset) {
        // Build a small JSON payload and ride the existing BLESend event.
        // Keeps watchdog plugin decoupled from BLEPlugin internals.
        JsonDocument doc;
        if (isReset) {
            doc[Protocol::FIELD_TYPE] = Protocol::EVT_WATCHDOG_RESET;
            doc["auto"]               = autoReset;
            doc["prev_cause"]         = getWatchdogCauseName(prevCause);
        } else {
            doc[Protocol::FIELD_TYPE] = Protocol::EVT_WATCHDOG_TRIPPED;
            doc["cause"]              = getWatchdogCauseName(cause);
            doc["temp"]               = gState.currentTemp;
            doc["unix"]               = gState.watchdogLastTripUnix;
        }
        String payload;
        serializeJson(doc, payload);
        bus().publish(EventType::BLESend, payload);
    }

    // ── esp_timer callback (loop-stuck detection) ───────────
    static void onTimerStatic(void* arg) {
        auto* self = static_cast<ThermalWatchdogPlugin*>(arg);
        if (!self) return;
        self->onTimer();
    }

    void onTimer() {
        // Already latched? Re-assert the GPIO LOW (cheap, idempotent) and
        // skip detection — the latch is what matters from here.
        if (gState.watchdogTripped) {
            gpio_set_level((gpio_num_t)PIN_HEATER_SSR, 0);
            return;
        }
#ifdef HIL_BUILD
        // HIL: detection suspended → ISR is observation-only.
        if (!_detectionsEnabled) return;
#endif
        // Compare against the last main-loop kick. Use millis() snapshot
        // — millis() is safe to call from the esp_timer task.
        uint32_t now = millis();
        if ((now - _heartbeatMs) >= gState.watchdogLoopStuckMs) {
            // Force the GPIO down RIGHT NOW. We cannot do the full trip()
            // here because it touches NVS/EventBus, which are not safe
            // from the timer task at unbounded latency. Set a flag and
            // let the main loop() drain it next iteration.
            gpio_set_level((gpio_num_t)PIN_HEATER_SSR, 0);
            _isrTripPending = true;
            bus().publish(EventType::WatchdogKick);  // diagnostic only
        }
    }

    // ── State ───────────────────────────────────────────────
    watchdog::SensorFaultTimer                              _sensorFault;
    watchdog::LoopHeartbeat                                 _heartbeat;
    watchdog::GradientDetector<WATCHDOG_GRAD_WINDOW_MAX>    _gradient;
    watchdog::CooldownTimer                                 _cooldown;

    IExternalCut*       _externalCut       = nullptr;
    esp_timer_handle_t  _isrTimer          = nullptr;
    volatile uint32_t   _heartbeatMs       = 0;
    volatile bool       _isrTripPending    = false;
    uint32_t            _lastSampleMs      = 0;
    bool                _gradTrippedThisTick = false;
#ifdef HIL_BUILD
    volatile bool       _detectionsEnabled = true;
#endif

    static ThermalWatchdogPlugin* _instance;
};

// Static instance for the C-style timer callback.
inline ThermalWatchdogPlugin* ThermalWatchdogPlugin::_instance = nullptr;
