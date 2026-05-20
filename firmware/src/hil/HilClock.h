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
//
// CRITICAL: hil_clock_now() is declared with C linkage and the EXACT same
// signature as Arduino's millis() (`unsigned long (*)(void)`). Otherwise
// downstream `extern "C" { unsigned long millis(void); }` declarations from
// Arduino.h get macro-rewritten into `unsigned long hil_clock_now(void)`
// and clash with a C++-linkage definition.

#ifdef HIL_BUILD

#ifdef __cplusplus
extern "C" {
#endif

// Real Arduino millis. Declared here so the implementation below can call
// the wall-clock symbol even after our macro is in effect.
unsigned long millis(void);

// Virtual clock now. Implementation lives in hil/HilClock.cpp.
unsigned long hil_clock_now(void);

#ifdef __cplusplus
}
#endif

// Override the millis() the rest of src/ sees. Has to be done AFTER the
// extern decls above so this header itself still resolves the real symbol.
#define millis() hil_clock_now()

#endif  // HIL_BUILD
