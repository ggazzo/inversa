#pragma once

// LCD bring-up for the Waveshare ESP32-S3-Touch-LCD-1.47 (JD9853,
// 172x320 portrait, AXS5106L touch). Uses Arduino_GFX_Library because
// LovyanGFX 1.x has no first-class JD9853 driver — Arduino_GFX is the
// stack that the board's reference code (VolosR/Wave147moreExamples)
// runs on, with a documented init sequence.
//
// We still hand the pixel buffer off to LVGL v9: the flush_cb in
// LCD.cpp calls Arduino_GFX::draw16bitRGBBitmap(). Throughput is lower
// than DMA push but plenty for our 30 FPS UI at 172x320.

#include <Arduino_GFX_Library.h>
#include <lvgl.h>

namespace inversa { namespace display {

// Pin map per the VolosR reference for the Waveshare board. The LCD reset
// line is actually pin 40 (Volos's LCD_RST); pin 47 is the touch reset
// (Touch_RST). Previous comment claiming they were shared was wrong, which
// is why the panel stayed dark — Arduino_ST7789 was toggling the touch
// reset instead of the LCD reset.
struct LcdPins {
    static constexpr int sck   = 38;
    static constexpr int mosi  = 39;
    static constexpr int dc    = 45;
    static constexpr int cs    = 21;
    static constexpr int rst   = 40;
    static constexpr int bl    = 46;
    static constexpr int sda   = 42;
    static constexpr int scl   = 41;
    static constexpr int tirq  = 48;
    // TP_RST is a separate pin from LCD_RST (40). Earlier we disabled it
    // thinking pulsing 47 was killing the LCD; turned out that was the
    // wrong touch read protocol locking the I2C bus, not a shared net.
    static constexpr int trst  = 47;
};

constexpr int LCD_W = 172;
constexpr int LCD_H = 320;

void lcd_init();
Arduino_GFX* gfx();

}}  // namespace inversa::display
