#pragma once

// HilState — shared globals consumed by the HIL test seams.
//
// HilHarnessPlugin writes to these from the JSONL command parser. The
// seams in TemperaturePlugin / AmbientSensorPlugin / RTCPlugin / Watchdog
// read them, gated by `#ifdef HIL_BUILD`. Production builds never see
// this file (it is included only behind that flag).

#ifdef HIL_BUILD

#include <stdint.h>

namespace hil {

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

    // Force-trip request — drained by ThermalWatchdogPlugin on its next
    // loop() tick. -1 = no pending trip.
    int8_t forceWatchdogCause = -1;
};

// Single global instance. Defined inline so the header alone is enough.
inline State g_hil{};

}  // namespace hil

#endif  // HIL_BUILD
