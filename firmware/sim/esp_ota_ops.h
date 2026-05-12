#pragma once
// esp_ota_ops.h stub — the simulator's main_sim.cpp does NOT invoke OTAPlugin
// or the rollback logic, but `src/main.cpp` references these symbols when
// SIM_BUILD is undefined. Under SIM_BUILD they're guarded out; this header
// keeps the include resolvable from any plugin that might transitively pull
// it in (e.g., a future plugin that probes partition state).

#include <cstdint>

typedef int  esp_err_t;
typedef int  esp_ota_img_states_t;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_ota_img_states_t ESP_OTA_IMG_PENDING_VERIFY = 0;

struct esp_partition_t {};

inline const esp_partition_t* esp_ota_get_running_partition() { return nullptr; }
inline esp_err_t esp_ota_get_state_partition(const esp_partition_t*, esp_ota_img_states_t*) { return -1; }
inline esp_err_t esp_ota_mark_app_valid_cancel_rollback() { return ESP_OK; }
