// AXS5106L touch — I2C poll-based driver feeding LVGL.
//
// Datasheet behavior used here:
//   - Single 8-byte status packet per read
//   - Byte 0 high nibble = touch count (0 or 1 for our needs)
//   - Bytes 4-5 = X (big-endian, 12-bit)
//   - Bytes 6-7 = Y (big-endian, 12-bit)
//
// We hold the last reported point and feed it to LVGL via the indev
// callback so a press-drag works correctly.

#include "Touch.h"
#include "LCD.h"
#include <Arduino.h>
#include <Wire.h>

namespace inversa { namespace display {

namespace {
// AXS5106L is configurable to 0x3B or 0x63 via strap; the Waveshare
// ESP32-S3-Touch-LCD-1.47 ships it at 0x63. We auto-detect both so a
// future board revision with the alternate strap keeps working.
constexpr uint8_t I2C_ADDR_PRIMARY = 0x63;
constexpr uint8_t I2C_ADDR_ALT     = 0x3B;
uint8_t s_addr = I2C_ADDR_PRIMARY;

uint16_t s_x = 0;
uint16_t s_y = 0;
bool     s_pressed = false;
bool     s_detected = false;

bool probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

void readPoint() {
    if (!s_detected) return;
    Wire.beginTransmission(s_addr);
    Wire.write(uint8_t(0x01));  // status register
    if (Wire.endTransmission(false) != 0) {
        s_pressed = false;
        return;
    }
    Wire.requestFrom(s_addr, uint8_t(8));
    uint8_t buf[8] = {0};
    for (size_t i = 0; i < 8 && Wire.available(); ++i) buf[i] = Wire.read();

    uint8_t count = buf[0] >> 4;
    if (count == 0) { s_pressed = false; return; }
    s_x = ((uint16_t(buf[4] & 0x0F) << 8) | buf[5]);
    s_y = ((uint16_t(buf[6] & 0x0F) << 8) | buf[7]);
    s_pressed = true;
}

void indev_read_cb(lv_indev_t* /*indev*/, lv_indev_data_t* data) {
    readPoint();
    if (s_pressed) {
        data->point.x = s_x;
        data->point.y = s_y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state   = LV_INDEV_STATE_RELEASED;
    }
}
}  // namespace

void touch_init() {
    // Optional reset pulse — only if the board exposes a dedicated TP
    // reset line. On Waveshare ESP32-S3-Touch-LCD-1.47 the TP_RST is
    // tied to LCD_RST and pulsed by LovyanGFX during lcd_init() already.
    if (LcdPins::trst >= 0) {
        pinMode(LcdPins::trst, OUTPUT);
        digitalWrite(LcdPins::trst, LOW);
        delay(10);
        digitalWrite(LcdPins::trst, HIGH);
        delay(50);
    }

    // 100 kHz first; bump to 400 kHz after a clean probe. Some chips
    // refuse the higher speed before their internal PLL settles.
    Wire.begin(LcdPins::sda, LcdPins::scl, 100000);
    delay(50);

    if (probe(I2C_ADDR_PRIMARY)) {
        s_addr = I2C_ADDR_PRIMARY;
        s_detected = true;
    } else if (probe(I2C_ADDR_ALT)) {
        s_addr = I2C_ADDR_ALT;
        s_detected = true;
    } else {
        Serial.println("[Touch] AXS5106L not found at 0x63 or 0x3B — touch disabled");
        return;
    }
    Wire.setClock(400000);
    Serial.printf("[Touch] AXS5106L detected at 0x%02X\n", s_addr);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_read_cb);
}

}}  // namespace inversa::display
