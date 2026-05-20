#pragma once

// BleClient — NimBLE central that pairs with the brewing controller.
//
// Protocol target: Nordic UART Service (NUS, 6E40…), same one the PWA
// uses. The controller advertises "Inversa" (BLE_DEVICE_NAME); we scan,
// pick the strongest matching device, connect, subscribe to the TX
// characteristic notifications, and parse evt:* JSON frames into
// AppState. Outbound commands (req:*) go through write() on the RX
// characteristic.
//
// One single FreeRTOS task owns this object; AppState fields are atomic
// or null-terminated chars updated under a short lock so the UI tick
// can read them without further coordination.

#include <string>

namespace inversa { namespace display {

void ble_init();
void ble_send(const char* json);  // raw NUS write (no rid wrapping yet)

// Convenience helpers — wrap a JSON command in a stable rid so retry
// logic upstream can match responses if/when we add that.
void ble_send_recipe_confirm();
void ble_send_recipe_pause();
void ble_send_recipe_resume();
void ble_send_recipe_stop();
void ble_send_watchdog_reset();
void ble_send_sched_stop();
void ble_send_set_temp(float c);
void ble_send_heater(bool on);

}}  // namespace inversa::display
