#pragma once

// HilClock — virtual clock for HIL builds.
//
// Under HIL_BUILD we redefine `millis()` to route to hil_clock_now(), which
// returns `real_millis() + g_offset_ms` (or a frozen snapshot when frozen).
// The host bridge can advance/freeze/set-epoch the offset via JSONL cmds,
// which lets us drive watchdog timeouts and scheduler windows in scaled
// virtual time while the firmware runs on real hardware.
//
// NOTE — only `millis()` is virtualized. `delay()`, `vTaskDelay`, `micros()`
// and ESP-IDF timer ticks intentionally stay on the real wall clock:
//   - delay()/vTaskDelay are FreeRTOS-scheduled and must keep real cadence
//     so other tasks (NimBLE host, watchdog timer task, etc.) run normally.
//   - micros() is consulted by drivers (ADC, I2C) where time is physical.
//   - esp_timer ticks at real time. Plugins that derive a *deadline* from
//     millis() (e.g. ThermalWatchdog::onTimer comparing now-_heartbeatMs)
//     still see virtual time correctly because both terms are virtualized.
//
// This file is force-included into every src/ translation unit via the
// `-include` build_src_flags entry in platformio.ini, so the redefinition
// is in effect everywhere src/ code sees `millis`. Framework libs and the
// arduino-esp32 internals continue to see the real `millis()` because they
// are compiled without this flag.

#ifdef HIL_BUILD

#include <stdint.h>

// Forward decl of the real Arduino millis(). We can't include Arduino.h here
// because this header is force-included before anything else and we'd hit a
// circular preprocessor mess. The real signature lives in esp32-hal-misc.c.
#ifdef __cplusplus
extern "C" {
#endif
unsigned long millis(void);  // real arduino-esp32 millis
#ifdef __cplusplus
}
#endif

namespace hil {

// Offset applied to wall millis(). Signed so we can rewind on epoch set.
// `volatile` because the timer task may read this concurrently with the
// main loop writing it; ESP32 32-bit aligned loads/stores are atomic.
inline volatile int32_t g_offset_ms = 0;

// When non-zero, hil_clock_now() returns `g_freeze_at` regardless of wall
// clock progress. Used by `clock freeze` to deterministically pause time.
inline volatile uint32_t g_freeze_at = 0;
inline volatile bool     g_frozen    = false;

}  // namespace hil

// Single virtual now() exposed to firmware code. Marked inline so it folds
// at every call site (millis() is on the PID/Watchdog hot paths).
static inline uint32_t hil_clock_now() {
    if (hil::g_frozen) return hil::g_freeze_at;
    return (uint32_t)((int32_t)millis() + hil::g_offset_ms);
}

// Override the millis() the rest of src/ sees. Done AFTER the extern decl
// above so this file itself still resolves the real symbol.
#define millis() hil_clock_now()

#endif  // HIL_BUILD
