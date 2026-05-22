#pragma once

// Shared color tokens for the Inversa display. Matches the dark-mode
// palette used by the PWA (Tamagui $accent / $danger / $warning).

#include <lvgl.h>

namespace inversa { namespace display {

inline lv_color_t theme_bg()        { return lv_color_hex(0x0B0B0F); }
inline lv_color_t theme_surface()   { return lv_color_hex(0x1A1A22); }
inline lv_color_t theme_primary()   { return lv_color_hex(0xFFFFFF); }
inline lv_color_t theme_muted()     { return lv_color_hex(0x6B7280); }
inline lv_color_t theme_accent()    { return lv_color_hex(0x10B981); }  // green
inline lv_color_t theme_warning()   { return lv_color_hex(0xF59E0B); }  // amber
inline lv_color_t theme_danger()    { return lv_color_hex(0xEF4444); }  // red
inline lv_color_t theme_info()      { return lv_color_hex(0x3B82F6); }  // blue

}}  // namespace inversa::display
