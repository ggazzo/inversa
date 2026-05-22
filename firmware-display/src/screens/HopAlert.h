#pragma once

#include <lvgl.h>

namespace inversa { namespace display {

// Modal hop-addition overlay. Created lazily on first show. Returns the
// overlay's root container — caller adds it on top of any screen.
void hop_alert_show(const char* name, int minMark);
void hop_alert_hide();
bool hop_alert_visible();

}}  // namespace inversa::display
