// lv_conf.h — minimal LVGL v9 config for the Inversa Display device.
//
// Tuned for the Waveshare ESP32-S3 1.47" board (172x320 IPS, JD9853).
// Color depth 16-bit RGB565, no anti-aliasing on small embedded fonts.
//
// PSRAM is available, so LVGL's render buffer and image cache can live
// outside internal RAM. The two render buffers (LV_DRAW_BUF_SIZE) get
// allocated from the heap by the application code in src/lcd/LCD.cpp.

#pragma once

#define LV_CONF_INCLUDE_SIMPLE 1

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 1

#define LV_USE_OS LV_OS_FREERTOS
#define LV_TICK_CUSTOM 0  // We feed lv_tick_inc from a FreeRTOS task.

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

// Memory: use the standard malloc, backed by PSRAM via ESP32 heap caps.
#define LV_USE_STDLIB_MALLOC LV_STDLIB_BUILTIN
#define LV_MEM_SIZE (96U * 1024U)

// Default fonts — keep just two so the binary stays modest. SquareLine
// will pull in extra .c font files as needed and reference them
// directly in screens that need them.
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

// Widgets actually used by the bridge + handwritten screens.
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_ARC 1
#define LV_USE_BAR 1
#define LV_USE_IMG 1
#define LV_USE_LINE 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_TILEVIEW 1

// Themes: keep the simple dark theme as default; SquareLine designs
// usually override this per-screen anyway.
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

#define LV_USE_TILEVIEW 1
