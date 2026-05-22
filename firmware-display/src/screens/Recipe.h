#pragma once

#include <lvgl.h>

namespace inversa { namespace display {

// Recipe view. Shows recipe name, current step, temp, progress bar.
// When recipeState == "wait_confirm", three buttons appear:
//   [ Confirmar ]  [ Pausar ]  [ Parar ]
// When recipeState == "paused" the Pausar button becomes [ Retomar ].
// Boil substate (boilActive) replaces progress bar with countdown.
lv_obj_t* recipe_screen_create();
void      recipe_screen_update();

}}  // namespace inversa::display
