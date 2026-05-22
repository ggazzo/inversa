#pragma once

#include <lvgl.h>

namespace inversa { namespace display {

// Watchdog trip overlay. Full-screen red with cause string and a Reset
// button. Shown on top of any screen when wdTripped is set.
void watchdog_overlay_show();
void watchdog_overlay_hide();
bool watchdog_overlay_visible();
void watchdog_overlay_update();

}}  // namespace inversa::display
