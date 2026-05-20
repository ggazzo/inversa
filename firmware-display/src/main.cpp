// Inversa Display — main entry.
//
// Boot order:
//   1. Serial for logs.
//   2. LovyanGFX panel + LVGL init.
//   3. AXS5106L touch driver + LVGL input device.
//   4. AppState placeholder (already singleton).
//   5. LvglBridge — registers the Scanning screen as initial view.
//   6. NimBLE central — kicks off scan for "Inversa" service.
//
// Loop:
//   - lv_timer_handler() at ~50 Hz (drives screen redraws + touch).
//   - bridge_tick() reads AppState and updates widgets.
//   - lv_tick_inc fed from a separate FreeRTOS task at 1 kHz so
//     animations stay smooth even when the main loop is doing IO.

#include <Arduino.h>
#include <lvgl.h>

#include "lcd/LCD.h"
#include "lcd/Touch.h"
#include "state/AppState.h"
#include "ble/BleClient.h"
#include "bridge/LvglBridge.h"

namespace {

using namespace inversa::display;

void lv_tick_task(void*) {
    constexpr uint32_t INC_MS = 5;
    for (;;) {
        lv_tick_inc(INC_MS);
        vTaskDelay(pdMS_TO_TICKS(INC_MS));
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n[Inversa Display] boot");

    lcd_init();
    touch_init();
    bridge_init();
    ble_init();

    xTaskCreatePinnedToCore(lv_tick_task, "lv_tick", 2048, nullptr, 1, nullptr, 0);

    Serial.println("[Inversa Display] ready");
}

void loop() {
    lv_timer_handler();
    inversa::display::bridge_tick();
    delay(20);
}
