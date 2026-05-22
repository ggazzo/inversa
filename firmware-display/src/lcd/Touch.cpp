// AXS5106L touch driver — I2C register read feeding LVGL.
//
// Protocol (taken from VolosR/Waveshare147Touch and the Espressif
// esp_lcd_touch_axs5106l component):
//   1. After power-up, hold TP_RST LOW 200 ms, then HIGH 300 ms.
//   2. To read touch data: write the single byte 0x01 (the
//      TOUCH_DATA_REG address), STOP, then requestFrom 14 bytes.
//   3. Layout: data[1] = touch count, data[2..5] = first touch
//      coordinates (x_hi nibble | x_lo, y_hi nibble | y_lo).
//
// Earlier this file sent an 8-byte "magic" sequence that doesn't exist
// in any known datasheet — the chip just NACK'd, and arduino-esp32's
// hal-ng I2C driver returned the TX buffer back as if it were the
// response (the "b5 ab a5 5a ..." echo we saw).

#include "Touch.h"
#include "LCD.h"
#include <Arduino.h>
#include <Wire.h>

namespace inversa { namespace display {

namespace {
constexpr uint8_t I2C_ADDR_PRIMARY = 0x63;
constexpr uint8_t I2C_ADDR_ALT     = 0x3B;
constexpr uint8_t REG_TOUCH_DATA   = 0x01;
constexpr uint8_t REG_ID           = 0x08;
constexpr size_t  READ_LEN         = 14;

uint8_t s_addr = I2C_ADDR_PRIMARY;
uint16_t s_x = 0;
uint16_t s_y = 0;
bool     s_pressed = false;
bool     s_detected = false;
volatile bool s_intFlag = false;

bool probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

bool i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return false;
    size_t got = Wire.requestFrom(addr, (uint8_t)len);
    if (got != len) return false;
    for (size_t i = 0; i < len; ++i) buf[i] = Wire.read();
    return true;
}

void IRAM_ATTR on_tp_int() { s_intFlag = true; }

void readPoint() {
    if (!s_detected) return;
    // Honor the INT line. AXS5106L pulls it LOW only while a finger is
    // on the glass; in that case our ISR sets s_intFlag. Polling I2C
    // when no touch is present just wastes bus time.
    if (!s_intFlag) { s_pressed = false; return; }
    s_intFlag = false;

    uint8_t buf[READ_LEN] = {0};
    if (!i2c_read_reg(s_addr, REG_TOUCH_DATA, buf, READ_LEN)) {
        s_pressed = false;
        return;
    }

    uint8_t count = buf[1] & 0x0F;
    if (count == 0) { s_pressed = false; return; }

    uint16_t raw_x = (uint16_t(buf[2] & 0x0F) << 8) | buf[3];
    uint16_t raw_y = (uint16_t(buf[4] & 0x0F) << 8) | buf[5];
    // Volos's example rotates per LCD rotation; we run portrait
    // (rotation 0) and the chip's native frame already matches the
    // panel orientation. Flip x if a future rotation flag changes
    // this.
    s_x = (raw_x >= LCD_W) ? (LCD_W - 1) : raw_x;
    s_y = (raw_y >= LCD_H) ? (LCD_H - 1) : raw_y;
    s_pressed = true;

    static uint8_t logged = 0;
    if (logged < 5) {
        Serial.printf("[Touch] count=%u raw=(%u,%u) → (%u,%u)\n",
                      count, raw_x, raw_y, s_x, s_y);
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
    if (!found) Serial.print(" (none)");
    Serial.println();
}

void touch_init() {
    // Reset the touch chip per the AXS5106L datasheet + Volos's example:
    // pulse TP_RST LOW for 200 ms then HIGH for 300 ms before any I2C.
    // The pin map now uses 47 again because the previous "tela escura"
    // failure was actually our wrong read protocol confusing the chip
    // and locking the bus, not a shared-reset issue.
    if (LcdPins::trst >= 0) {
        pinMode(LcdPins::trst, OUTPUT);
        digitalWrite(LcdPins::trst, LOW);
        delay(200);
        digitalWrite(LcdPins::trst, HIGH);
        delay(300);
    }
    if (LcdPins::tirq >= 0) {
        pinMode(LcdPins::tirq, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(LcdPins::tirq), on_tp_int, FALLING);
    }

    Wire.begin(LcdPins::sda, LcdPins::scl, 400000);
    delay(50);

    scan_i2c();

    if (probe(I2C_ADDR_PRIMARY))      { s_addr = I2C_ADDR_PRIMARY; s_detected = true; }
    else if (probe(I2C_ADDR_ALT))     { s_addr = I2C_ADDR_ALT;     s_detected = true; }
    else {
        Serial.println("[Touch] AXS5106L not found — touch disabled");
        return;
    }
    Serial.printf("[Touch] AXS5106L detected at 0x%02X\n", s_addr);

    // Optional: read the ID register so we can confirm the chip is
    // really alive (anything past NACK echoes would print zero/garbage).
    uint8_t id[3] = {0};
    if (i2c_read_reg(s_addr, REG_ID, id, 3)) {
        Serial.printf("[Touch] ID bytes: %02X %02X %02X\n", id[0], id[1], id[2]);
    }

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_read_cb);
}

}}  // namespace inversa::display
