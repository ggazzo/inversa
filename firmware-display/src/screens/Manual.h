#pragma once

#include <lvgl.h>

namespace inversa { namespace display {

// Manual mode view: target temperature slider (22..100 °C) + heater toggle.
// Tapping the slider commits a set-temp; toggle hits ble_send_heater.
lv_obj_t* manual_screen_create();
void      manual_screen_update();

}}  // namespace inversa::display
