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
#include <Wire.h>

namespace inversa { namespace display {

namespace {
constexpr uint8_t I2C_ADDR = 0x3B;

uint16_t s_x = 0;
uint16_t s_y = 0;
bool     s_pressed = false;

void readPoint() {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(uint8_t(0x01));  // status register
    if (Wire.endTransmission(false) != 0) {
        s_pressed = false;
        return;
    }
    Wire.requestFrom(I2C_ADDR, uint8_t(8));
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
    Wire.begin(LcdPins::sda, LcdPins::scl, 400000);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_read_cb);
}

}}  // namespace inversa::display
