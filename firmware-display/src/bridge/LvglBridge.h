#pragma once

// LvglBridge — pulls fields from AppState and pushes them into LVGL
// widget objects. Runs on the same task as LVGL itself; called from the
// main render tick.
//
// For the scaffold this only switches between the handwritten Scanning
// screen and a placeholder Home screen. The intent is that the
// SquareLine-generated screens (under src/ui/) will register their own
// LVGL objects and the bridge will look them up by name and update
// them with values from AppState every tick.

#include <lvgl.h>

namespace inversa { namespace display {

void bridge_init();
void bridge_tick();   // called from the main loop after lv_timer_handler

}}  // namespace inversa::display
