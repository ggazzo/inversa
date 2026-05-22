#pragma once

#include <lvgl.h>

namespace inversa { namespace display {

// Home / idle view. Big current temperature, target, mode label, plus
// heater/pump LED indicators. No buttons — primary control is on the PWA.
lv_obj_t* home_screen_create();
void      home_screen_update();

}}  // namespace inversa::display
