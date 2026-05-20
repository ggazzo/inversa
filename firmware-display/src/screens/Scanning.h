#pragma once

// Scanning.h — handwritten LVGL screen shown while we're scanning for
// the brewing controller. Replaced (or wrapped) by the SquareLine
// export under src/ui/ once that is generated.

#include <lvgl.h>

namespace inversa { namespace display {

lv_obj_t* scanning_screen_create();

}}  // namespace inversa::display
