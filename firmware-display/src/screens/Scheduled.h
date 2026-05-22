#pragma once

#include <lvgl.h>

namespace inversa { namespace display {

// Scheduled-brew view. Shows target time + temp + volume, countdown,
// status line. Single button: Cancelar (stop schedule).
lv_obj_t* scheduled_screen_create();
void      scheduled_screen_update();

}}  // namespace inversa::display
