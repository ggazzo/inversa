// LCD glue — LovyanGFX panel init + LVGL display driver registration.
//
// LVGL v9 expects a display handle bound to two functions: flush_cb
// (push a rectangle of pixels to hardware) and a tick mechanism. The
// tick is driven from a FreeRTOS task in main.cpp; this file only
// handles flush.
//
// Buffers: two row-tall partial buffers in PSRAM. LVGL renders into
// one while the other is being DMA'd to the panel. Sizing one full row
// (172 px × 2 bytes = 344 B) keeps the worst-case latency low while
// still letting LVGL batch many widget updates per flush.

#include "LCD.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <initializer_list>

namespace inversa { namespace display {

WaveshareS3Lcd::WaveshareS3Lcd() {
    {
        auto cfg = _bus.config();
        cfg.spi_host    = SPI2_HOST;
        cfg.spi_mode    = 0;
        cfg.freq_write  = 40 * 1000 * 1000;
        cfg.freq_read   = 16 * 1000 * 1000;
        cfg.spi_3wire   = false;
        cfg.use_lock    = true;
        cfg.dma_channel = SPI_DMA_CH_AUTO;
        cfg.pin_sclk    = LcdPins::sck;
        cfg.pin_mosi    = LcdPins::mosi;
        cfg.pin_miso    = LcdPins::miso;
        cfg.pin_dc      = LcdPins::dc;
        _bus.config(cfg);
        _panel.setBus(&_bus);
    }
    {
        auto cfg = _panel.config();
        cfg.pin_cs           = LcdPins::cs;
        cfg.pin_rst          = LcdPins::rst;
        cfg.pin_busy         = -1;
        cfg.panel_width      = LCD_W;
        cfg.panel_height     = LCD_H;
        cfg.offset_x         = 34;   // JD9853 172x320 typical offsets
        cfg.offset_y         = 0;
        cfg.offset_rotation  = 0;
        cfg.dummy_read_pixel = 8;
        cfg.dummy_read_bits  = 1;
        cfg.readable         = false;
        cfg.invert           = true;
        cfg.rgb_order        = false;
        cfg.dlen_16bit       = false;
        cfg.bus_shared       = false;
        _panel.config(cfg);
    }
    {
        auto cfg = _light.config();
        cfg.pin_bl      = LcdPins::bl;
        cfg.invert      = false;
        cfg.freq        = 44100;
        cfg.pwm_channel = 7;
        _light.config(cfg);
        _panel.setLight(&_light);
    }
    setPanel(&_panel);
}

static WaveshareS3Lcd s_lcd;
WaveshareS3Lcd& lcd() { return s_lcd; }

// LVGL v9 flush callback. The bus is set up to DMA, so we hand the
// pixels off and signal flush-ready immediately — LovyanGFX queues the
// transfer and we don't block the LVGL render thread.
static void disp_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
    const int32_t w = lv_area_get_width(area);
    const int32_t h = lv_area_get_height(area);
    s_lcd.startWrite();
    s_lcd.setAddrWindow(area->x1, area->y1, w, h);
    s_lcd.pushPixelsDMA(reinterpret_cast<uint16_t*>(px), w * h);
    s_lcd.endWrite();
    lv_display_flush_ready(disp);
}

// JD9853-specific init sequence. Transcribed from
// VolosR/Wave147moreExamples/TurboBlink (Arduino_GFX DSL form). We
// issue these AFTER LovyanGFX's Panel_ST7789 init has set up the SPI
// bus and CS/DC pins; LovyanGFX's generic ST7789 commands largely
// match the JD9853 startup path, but the panel-specific gamma /
// vcom / source-driver settings are unique. Without these bytes the
// panel powers on, ack's commands, and stays uniformly dark.
static void jd9853_init() {
    auto& d = s_lcd;
    auto cmd = [&](uint8_t c) { d.writeCommand(c); };
    auto dat = [&](uint8_t v) { d.writeData(v); };
    auto datN = [&](std::initializer_list<uint8_t> xs) { for (auto x : xs) d.writeData(x); };

    d.startWrite();
    cmd(0x11); delay(120);                                        // sleep out

    cmd(0xDF); datN({0x98, 0x53});                                // page select
    cmd(0xB2); dat(0x23);

    cmd(0xB7); datN({0x00, 0x47, 0x00, 0x6F});                    // porch
    cmd(0xBB); datN({0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0});

    cmd(0xC0); datN({0x44, 0xA4});                                // power ctrl
    cmd(0xC1); dat(0x16);

    cmd(0xC3); datN({0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77});
    cmd(0xC4); datN({0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82});

    cmd(0xC8); datN({                                              // gamma
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
    });

    cmd(0xD0); datN({0x04, 0x06, 0x6B, 0x0F, 0x00});
    cmd(0xD7); datN({0x00, 0x30});
    cmd(0xE6); dat(0x14);
    cmd(0xDE); dat(0x01);

    cmd(0xB7); datN({0x03, 0x13, 0xEF, 0x35, 0x35});
    cmd(0xC1); datN({0x14, 0x15, 0xC0});
    cmd(0xC2); datN({0x06, 0x3A});
    cmd(0xC4); datN({0x72, 0x12});
    cmd(0xBE); dat(0x00);
    cmd(0xDE); dat(0x02);

    cmd(0xE5); datN({0x00, 0x02, 0x00});
    cmd(0xE5); datN({0x01, 0x02, 0x00});

    cmd(0xDE); dat(0x00);
    cmd(0x35); dat(0x00);                                          // TE off
    cmd(0x3A); dat(0x05);                                          // 16-bit RGB565

    cmd(0x36); dat(0x00);                                          // MADCTL portrait

    cmd(0x21);                                                     // invert ON
    cmd(0x11); delay(120);                                         // sleep out (again, per ref)
    cmd(0x29);                                                     // display ON
    d.endWrite();
}

void lcd_init() {
    pinMode(LcdPins::bl, OUTPUT);
    digitalWrite(LcdPins::bl, HIGH);

    bool ok = s_lcd.init();
    Serial.printf("[LCD] init() -> %d\n", ok ? 1 : 0);

    // Panel_ST7789 only takes us partway; the JD9853 needs its own
    // gamma/source-driver bytes to actually emit light through pixels.
    jd9853_init();

    s_lcd.setBrightness(255);

    // Bring-up smoke test.
    s_lcd.fillScreen(TFT_RED);   delay(400);
    s_lcd.fillScreen(TFT_GREEN); delay(400);
    s_lcd.fillScreen(TFT_BLUE);  delay(400);
    s_lcd.fillScreen(TFT_BLACK);

    lv_init();

    // Allocate two partial draw buffers in PSRAM. ~10 KB each: enough
    // to amortize the LVGL render bookkeeping but small enough that we
    // never sit on a long blocking transfer.
    constexpr size_t LINES_PER_BUF = 40;
    constexpr size_t BUF_PX = LCD_W * LINES_PER_BUF;
    constexpr size_t BUF_BYTES = BUF_PX * sizeof(uint16_t);

    auto* buf1 = static_cast<uint8_t*>(heap_caps_malloc(BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM));
    auto* buf2 = static_cast<uint8_t*>(heap_caps_malloc(BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM));

    lv_display_t* disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, BUF_BYTES, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

}}  // namespace inversa::display
