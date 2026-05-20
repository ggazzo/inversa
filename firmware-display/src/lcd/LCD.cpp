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
#include <esp_heap_caps.h>

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

void lcd_init() {
    s_lcd.init();
    s_lcd.setBrightness(180);
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
