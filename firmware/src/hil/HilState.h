#pragma once

// HilState — shared globals consumed by the HIL test seams and the
// virtual clock implementation.
//
// HilHarnessPlugin writes to these from the JSONL command parser. The
// seams in TemperaturePlugin / AmbientSensorPlugin / RTCPlugin / Watchdog
// read them, gated by `#ifdef HIL_BUILD`. The clock state is read by
// hil_clock_now() in HilClock.cpp. Production builds never see this file
// (it is included only behind the HIL_BUILD flag).

#ifdef HIL_BUILD

#include <stdint.h>

namespace hil {

// ── Clock state ────────────────────────────────────────────
// `volatile` because the ESP timer task may sample these concurrently
// with the main loop writing them. 32-bit aligned loads/stores are atomic
// on xtensa-esp-elf so no further synchronization is needed.
struct ClockState {
    volatile int32_t  offsetMs = 0;
    volatile uint32_t freezeAt = 0;
    volatile bool     frozen   = false;
};

// Single global instance. inline = single storage across all TUs.
inline ClockState g_clock{};

// ── Harness state (sensor overrides, forced trips) ─────────
struct State {
    // NTC override — when ntcActive=true, TemperaturePlugin bypasses the
    // analogRead path and feeds ntcCelsius into the Kalman filter as if it
    // were the freshly-converted ADC sample. If ntcBypassKalman=true the
    // Kalman is skipped entirely (the value lands straight in gState).
    bool  ntcActive          = false;
    float ntcCelsius         = 22.0f;
    bool  ntcBypassKalman    = false;

    // Ambient sensor override — the harness can drive
    // gState.ambientSensorC / ambientSensorOk directly.
    bool  ambientActive      = false;
    float ambientCelsius     = 22.0f;
    bool  ambientOk          = false;

    // RTC override — virtual unix epoch. RTCPlugin reads this instead of
    // hitting the DS1307 when rtcActive=true.
    bool     rtcActive       = false;
    uint32_t rtcEpoch        = 0;
    // millis() snapshot at the moment rtcEpoch was set, so the virtual RTC
    // advances second-by-second with the virtual clock.
    uint32_t rtcEpochSetAtMs = 0;
};

inline State g_hil{};

}  // namespace hil

#endif  // HIL_BUILD
