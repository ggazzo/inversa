// LCD bring-up via Arduino_GFX_Library + JD9853-specific init bytes.
//
// Arduino_GFX gives us a working SPI bus to the panel and a generic
// ST7789 envelope. The JD9853 needs additional source-driver / gamma
// commands on top; lcd_reg_init() below is verbatim from
// VolosR/Wave147moreExamples/TurboBlink.ino, which is the only fully
// working example for this exact Waveshare board.
//
// LVGL v9 flush callback at the bottom hands rectangles to
// draw16bitRGBBitmap. The render buffers are allocated in PSRAM from
// the heap.

#include "LCD.h"
#include <Arduino.h>
#include <esp_heap_caps.h>

namespace inversa { namespace display {

namespace {

Arduino_DataBus* s_bus = nullptr;
Arduino_GFX*     s_gfx = nullptr;

void jd9853_init() {
    // VERBATIM batch from VolosR/Wave147moreExamples/TurboBlink.ino —
    // proven working on this exact Waveshare board. Using Arduino_GFX's
    // batchOperation DSL (BEGIN_WRITE / WRITE_COMMAND_8 / WRITE_C8_D8 /
    // WRITE_C8_D16 / WRITE_BYTES <n> / DELAY <ms> / END_WRITE) ensures
    // CS/DC pin transitions are handled identically to Volos's code.
    static const uint8_t init_operations[] = {
        BEGIN_WRITE,
        WRITE_COMMAND_8, 0x11,
        END_WRITE,
        DELAY, 120,
        BEGIN_WRITE,
        WRITE_C8_D16, 0xDF, 0x98, 0x53,
        WRITE_C8_D8,  0xB2, 0x23,
        WRITE_COMMAND_8, 0xB7,
        WRITE_BYTES, 4, 0x00, 0x47, 0x00, 0x6F,
        WRITE_COMMAND_8, 0xBB,
        WRITE_BYTES, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
        WRITE_C8_D16, 0xC0, 0x44, 0xA4,
        WRITE_C8_D8,  0xC1, 0x16,
        WRITE_COMMAND_8, 0xC3,
        WRITE_BYTES, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
        WRITE_COMMAND_8, 0xC4,
        WRITE_BYTES, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
        WRITE_COMMAND_8, 0xC8,
        WRITE_BYTES, 32, 0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
                         0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28, 0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
        WRITE_COMMAND_8, 0xD0,
        WRITE_BYTES, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
        WRITE_C8_D16, 0xD7, 0x00, 0x30,
        WRITE_C8_D8,  0xE6, 0x14,
        WRITE_C8_D8,  0xDE, 0x01,
        WRITE_COMMAND_8, 0xB7,
        WRITE_BYTES, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
        WRITE_COMMAND_8, 0xC1,
        WRITE_BYTES, 3, 0x14, 0x15, 0xC0,
        WRITE_C8_D16, 0xC2, 0x06, 0x3A,
        WRITE_C8_D16, 0xC4, 0x72, 0x12,
        WRITE_C8_D8,  0xBE, 0x00,
        WRITE_C8_D8,  0xDE, 0x02,
        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3, 0x00, 0x02, 0x00,
        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3, 0x01, 0x02, 0x00,
        WRITE_C8_D8,  0xDE, 0x00,
        WRITE_C8_D8,  0x35, 0x00,
        WRITE_C8_D8,  0x3A, 0x05,
        WRITE_COMMAND_8, 0x2A,
        WRITE_BYTES, 4, 0x00, 0x22, 0x00, 0xCD,   // col 34..205 (172 px)
        WRITE_COMMAND_8, 0x2B,
        WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0x3F,   // row 0..319 (320 px)
        WRITE_C8_D8,  0xDE, 0x02,
        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3, 0x00, 0x02, 0x00,
        WRITE_C8_D8,  0xDE, 0x00,
        WRITE_C8_D8,  0x36, 0x00,
        WRITE_COMMAND_8, 0x21,
        END_WRITE,
        DELAY, 10,
        BEGIN_WRITE,
        WRITE_COMMAND_8, 0x29,
        END_WRITE,
    };
    s_bus->batchOperation(init_operations, sizeof(init_operations));
}

void disp_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px) {
    const int32_t w = lv_area_get_width(area);
    const int32_t h = lv_area_get_height(area);
    // Arduino_GFX::draw16bitRGBBitmap pushes each uint16_t as [high, low]
    // over SPI, so a host-order RGB565 value (LVGL with LV_COLOR_16_SWAP=0)
    // lands on the panel as written — no manual swap needed.
    s_gfx->draw16bitRGBBitmap(area->x1, area->y1,
                              reinterpret_cast<uint16_t*>(px), w, h);
    lv_display_flush_ready(disp);
}

}  // namespace

Arduino_GFX* gfx() { return s_gfx; }

void lcd_init() {
    pinMode(LcdPins::bl, OUTPUT);
    digitalWrite(LcdPins::bl, HIGH);

    s_bus = new Arduino_ESP32SPI(LcdPins::dc, LcdPins::cs,
                                 LcdPins::sck, LcdPins::mosi);
    // Constructor args mirror Volos's example: ST7789 envelope, RST 47,
    // rotation 0, IPS=false, 172x320 with the 34-px column offset both
    // sides that the JD9853 needs to land pixel column 0 at GRAM 34.
    s_gfx = new Arduino_ST7789(s_bus, LcdPins::rst, /*rotation*/ 0,
                               /*IPS*/ false,
                               LCD_W, LCD_H,
                               /*col_off1*/ 34, /*row_off1*/ 0,
                               /*col_off2*/ 34, /*row_off2*/ 0);

    if (!s_gfx->begin()) {
        Serial.println("[LCD] gfx->begin() FAILED");
    } else {
        Serial.println("[LCD] gfx->begin() ok");
    }

    // JD9853-specific init AFTER the generic ST7789 init so the
    // panel-specific gamma / source-driver bytes overlay it.
    jd9853_init();
    Serial.println("[LCD] JD9853 init applied");

    // Smoke test — should now actually render colors. RGB565 literals so
    // we don't depend on whichever name macros the GFX fork exposes.
    s_gfx->fillScreen(0xF800);  delay(400);  // red
    s_gfx->fillScreen(0x07E0);  delay(400);  // green
    s_gfx->fillScreen(0x001F);  delay(400);  // blue
    s_gfx->fillScreen(0x0000);              // black

    lv_init();

    // Render buffer allocation moved to internal RAM. PSRAM-backed DMA
    // can corrupt under SPI burst pushes on the ESP32-S3 octal PSRAM
    // variant unless cache alignment is guaranteed; internal SRAM is
    // simpler and we have plenty of headroom (RAM usage stays under
    // 50%). 20 lines × 172 px × 2 bytes = ~7 KB per buffer.
    constexpr size_t LINES_PER_BUF = 20;
    constexpr size_t BUF_PX = LCD_W * LINES_PER_BUF;
    constexpr size_t BUF_BYTES = BUF_PX * sizeof(uint16_t);
    auto* buf1 = static_cast<uint8_t*>(heap_caps_malloc(BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    auto* buf2 = static_cast<uint8_t*>(heap_caps_malloc(BUF_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    Serial.printf("[LCD] buf1=%p buf2=%p (%u bytes each)\n",
                  (void*)buf1, (void*)buf2, (unsigned)BUF_BYTES);

    lv_display_t* disp = lv_display_create(LCD_W, LCD_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, BUF_BYTES,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
}

}}  // namespace inversa::display
