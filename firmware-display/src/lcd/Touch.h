#pragma once

// AXS5106L capacitive touch controller — minimal I2C driver +
// LVGL input device registration. The chip presents an I2C interface
// at address 0x3B (Waveshare default; some PCB revisions strap it
// elsewhere). A single "tap" is read as one point with absolute coords;
// gestures and multi-touch are ignored.
//
// LVGL polls this driver at the rate it sets on the input device. We
// configure 30 Hz polling, which is plenty for the ~5 widgets per
// screen and keeps the I2C bus quiet for the rest of the system.

#include <lvgl.h>

namespace inversa { namespace display {

void touch_init();

}}  // namespace inversa::display
