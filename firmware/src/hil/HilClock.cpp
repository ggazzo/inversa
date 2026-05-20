// HilClock implementation. Only built under HIL_BUILD.
//
// This TU must call the *real* arduino-esp32 millis(). Since
// HilClock.h is force-included by build_src_flags, it has already done
// `#define millis() hil_clock_now()`. We undef the macro here BEFORE
// declaring the real symbol, so this file (and only this file) sees the
// wall-clock function under its native name.

#ifdef HIL_BUILD

#include <stdint.h>

#undef millis

// Real Arduino millis(). The framework provides the definition.
extern "C" unsigned long millis(void);

// Clock state lives in hil:: namespace.
#include "HilState.h"

// C linkage to match the declaration in HilClock.h. Returning `unsigned
// long` (not uint32_t) keeps the signature identical to Arduino's.
extern "C" unsigned long hil_clock_now(void) {
    if (hil::g_clock.frozen) return (unsigned long)hil::g_clock.freezeAt;
    return (unsigned long)((int32_t)millis() + hil::g_clock.offsetMs);
}

#endif  // HIL_BUILD
