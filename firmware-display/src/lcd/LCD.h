#pragma once

// LCD bring-up for the Waveshare ESP32-S3 1.47" board.
//
// Driver: JD9853 (172x320 IPS portrait). LovyanGFX provides a generic
// JD9853 panel class; this header wires its pin map for the Waveshare
// reference design. If the board you have rev-swaps pins, override the
// macros below.
//
// The LovyanGFX LGFX_Device instance also holds the framebuffer that
// LVGL flushes into via the disp_flush() callback registered in
// LCD.cpp. We use two heap-allocated draw buffers sized to one row (172
// pixels × 2 bytes) for low memory pressure; the buffer can be enlarged
// later if redraw latency becomes a concern.

#include <LovyanGFX.hpp>
#include <lvgl.h>

namespace inversa { namespace display {

// Pin map — verified against the Waveshare ESP32-S3 1.47" "LCD Touch"
// schematic. If you have the SD-only variant, the pins differ; see the
// README at firmware-display/README.md for the override knobs.
// Pin map per the Waveshare ESP32-S3-Touch-LCD-1.47 wiki schematic
// (rev 1.0, JD9853 + AXS5106L). If your board silkscreen differs (older
// pre-touch rev or third-party clone) tweak the constants here.
// Pin map matches the VolosR/Wave147moreExamples reference for the
// Waveshare ESP32-S3-Touch-LCD-1.47, JD9853 + AXS5106L variant.
// LCD_RST is shared with TP_RST on this board, so we leave trst at -1
// and let LovyanGFX pulse the reset line during lcd_init.
struct LcdPins {
    static constexpr int sck   = 38;
    static constexpr int mosi  = 39;
    static constexpr int miso  = -1;
    static constexpr int dc    = 45;
    static constexpr int cs    = 21;
    static constexpr int rst   = 47;
    static constexpr int bl    = 46;
    static constexpr int sda   = 42;
    static constexpr int scl   = 41;
    static constexpr int tirq  = 48;
    static constexpr int trst  = -1;  // shared with LCD_RST on pin 47
};

constexpr int LCD_W = 172;
constexpr int LCD_H = 320;

// LovyanGFX device subclass — bound at compile time so the panel/bus
// init runs without runtime dispatch.
//
// NOTE — LovyanGFX 1.2.21 ships no first-class Panel_JD9853; we use
// Panel_ST7789 as the closest compatible driver (same SPI command set
// for the 172×320 init path that Waveshare ships). If colors look
// wrong on real hardware, drop the bespoke init bytes from Waveshare's
// reference firmware into a custom subclass — typed-out init sequence
// goes in writeCommand_impl. TODO before shipping to brewery.
class WaveshareS3Lcd : public lgfx::LGFX_Device {
public:
    WaveshareS3Lcd();

private:
    lgfx::Panel_ST7789       _panel;
    lgfx::Bus_SPI            _bus;
    lgfx::Light_PWM          _light;
};

// Lifecycle.
void lcd_init();
WaveshareS3Lcd& lcd();

}}  // namespace inversa::display
