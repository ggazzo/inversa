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

// AXS5106L proper read protocol (mirrors Espressif's
// esp_lcd_touch_axs5106l component): write an 8-byte read-touch command,
// then read 14 bytes containing up to 1 point. A naive register read
// (write addr, read N) NACKs every time — the chip ignores anything
// that's not the magic command sequence.
void readPoint() {
    if (!s_detected) return;

    // The AXS5106L drives TP_INT LOW only while a finger is on the
    // glass. Polling without honoring this returns stale/spurious bytes
    // (we saw 1370,0 looping). Gate the I2C read on the INT line so
    // we only ask the chip for coords when it actually has them.
    if (LcdPins::tirq >= 0 && digitalRead(LcdPins::tirq) == HIGH) {
        s_pressed = false;
        return;
    }

    static const uint8_t READ_CMD[8] = {
        0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08
    };

    Wire.beginTransmission(s_addr);
    Wire.write(READ_CMD, sizeof(READ_CMD));
    if (Wire.endTransmission() != 0) {
        s_pressed = false;
        return;
    }

    uint8_t buf[14] = {0};
    size_t got = Wire.requestFrom(s_addr, uint8_t(sizeof(buf)));
    for (size_t i = 0; i < got && Wire.available(); ++i) buf[i] = Wire.read();
    if (got < 6) { s_pressed = false; return; }

    // Layout from the BSP driver:
    //   buf[1] high nibble = touch count (0 / 1)
    //   buf[2] high nibble | buf[3] = X (12 bit)
    //   buf[4] high nibble | buf[5] = Y (12 bit)
    uint8_t count = (buf[1] & 0x0F);
    if (count == 0) { s_pressed = false; return; }

    // The AXS5106L reports 12-bit raw coordinates over the chip's
    // internal sensor grid (~0..4095). Scale to the LCD's pixel
    // dimensions before handing to LVGL, otherwise indev_pointer_proc
    // complains the point is outside hor/ver resolution.
    constexpr uint16_t MAX_RAW = 4095;
    uint16_t raw_x = (uint16_t(buf[2] & 0x0F) << 8) | buf[3];
    uint16_t raw_y = (uint16_t(buf[4] & 0x0F) << 8) | buf[5];
    s_x = uint16_t((uint32_t)raw_x * (LCD_W - 1) / MAX_RAW);
    s_y = uint16_t((uint32_t)raw_y * (LCD_H - 1) / MAX_RAW);
    s_pressed = true;

    // Diagnostic: print the first few touches so the operator can
    // verify orientation (corner-tap test). Throttled to ~5 prints.
    static uint8_t logged = 0;
    if (logged < 5) {
        Serial.printf("[Touch] raw=(%u,%u) scaled=(%u,%u)\n", raw_x, raw_y, s_x, s_y);
        ++logged;
    }
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

// Scan the bus and print every responder so we can see at boot which
// chip is actually wired. Useful when the silkscreen documentation
// disagrees with the board revision in front of us.
static void scan_i2c() {
    Serial.printf("[Touch] I2C scan (SDA=%d SCL=%d):", LcdPins::sda, LcdPins::scl);
    int found = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf(" 0x%02X", addr);
            ++found;
        }
    }
    if (found == 0) Serial.print(" (no responders)");
    Serial.println();
}

void touch_init() {
    // INT line is open-drain low-active on AXS5106L. Pull-up needed
    // so HIGH reads as no-touch.
    if (LcdPins::tirq >= 0) {
        pinMode(LcdPins::tirq, INPUT_PULLUP);
    }

    if (LcdPins::trst >= 0) {
        pinMode(LcdPins::trst, OUTPUT);
        digitalWrite(LcdPins::trst, LOW);
        delay(10);
        digitalWrite(LcdPins::trst, HIGH);
        delay(50);
    }

    Wire.begin(LcdPins::sda, LcdPins::scl, 100000);
    delay(50);

    scan_i2c();  // logs all chips on the bus regardless of expected addr

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
