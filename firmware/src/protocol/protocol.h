#pragma once

// ─── BLE Protocol ───────────────────────────────────────────
// All messages are JSON objects with a "tp" (type) field.
// Request IDs use "rid" for correlation.
//
// Direction: Device → App (telemetry/events/responses)
// Direction: App → Device (commands/requests)

namespace Protocol {

    // ── Message Types (tp field) ────────────────────────────
    
    // Telemetry (device → app, periodic)
    constexpr const char* EVT_STATUS        = "evt:status";
    
    // Events (device → app)
    constexpr const char* EVT_RECIPE_STEP    = "evt:recipe:step";
    constexpr const char* EVT_RECIPE_STATE   = "evt:recipe:state";
    constexpr const char* EVT_RECIPE_CONFIRM = "evt:recipe:confirm";
    constexpr const char* EVT_RECIPE_RECOVERY = "evt:recipe:recovery";
    constexpr const char* EVT_ERROR          = "evt:error";
    constexpr const char* EVT_LOG            = "evt:log";

    // Requests (app → device)
    constexpr const char* REQ_SET_TEMP      = "req:set-temp";
    constexpr const char* REQ_SET_PID       = "req:set-pid";
    constexpr const char* REQ_HEATER_ON     = "req:heater:on";
    constexpr const char* REQ_HEATER_OFF    = "req:heater:off";
    constexpr const char* REQ_PUMP_ON       = "req:pump:on";
    constexpr const char* REQ_PUMP_OFF      = "req:pump:off";
    constexpr const char* REQ_RECIPE_LIST   = "req:recipe:list";
    constexpr const char* REQ_RECIPE_LOAD   = "req:recipe:load";
    constexpr const char* REQ_RECIPE_START  = "req:recipe:start";
    constexpr const char* REQ_RECIPE_STOP   = "req:recipe:stop";
    constexpr const char* REQ_RECIPE_PAUSE  = "req:recipe:pause";
    constexpr const char* REQ_RECIPE_RESUME = "req:recipe:resume";
    constexpr const char* REQ_RECIPE_CONFIRM = "req:recipe:confirm";
    constexpr const char* REQ_RECIPE_SAVE   = "req:recipe:save";
    constexpr const char* REQ_RECIPE_DELETE = "req:recipe:delete";
    constexpr const char* REQ_SETTINGS_GET  = "req:settings:get";
    constexpr const char* REQ_SETTINGS_SET  = "req:settings:set";
    // P13 — persisted thermal params (volume, heater power, ambient, vessel)
    constexpr const char* REQ_SETTINGS_THERMAL_GET = "req:settings:thermal:get";
    constexpr const char* REQ_SETTINGS_THERMAL_SET = "req:settings:thermal:set";
    // P7 — guarded factory reset (requires {confirm: "ERASE_ALL"})
    constexpr const char* REQ_FACTORY_RESET = "req:factory:reset";
    constexpr const char* REQ_STATUS        = "req:status";
    constexpr const char* REQ_INFO          = "req:info";
    constexpr const char* REQ_AUTOTUNE_START = "req:autotune:start";
    constexpr const char* REQ_AUTOTUNE_STOP  = "req:autotune:stop";
    constexpr const char* REQ_RECOVERY_RESUME  = "req:recovery:resume";
    constexpr const char* REQ_RECOVERY_DISCARD = "req:recovery:discard";

    // WiFi (app → device)
    constexpr const char* REQ_WIFI_CONFIG     = "req:wifi:config";
    constexpr const char* REQ_WIFI_CONNECT    = "req:wifi:connect";
    constexpr const char* REQ_WIFI_DISCONNECT = "req:wifi:disconnect";
    constexpr const char* REQ_WIFI_STATUS     = "req:wifi:status";

    // OTA (app → device)
    constexpr const char* REQ_OTA_CHECK       = "req:ota:check";
    constexpr const char* REQ_OTA_INSTALL     = "req:ota:install";

    // Ramp Mode (app → device)
    constexpr const char* REQ_RAMP_SET        = "req:ramp:set";     // {rate: float} °C/min, 0 = disabled
    constexpr const char* REQ_RAMP_STOP       = "req:ramp:stop";

    // Brew Log (app → device)
    constexpr const char* REQ_LOG_START       = "req:log:start";
    constexpr const char* REQ_LOG_STOP        = "req:log:stop";
    constexpr const char* REQ_LOG_EXPORT      = "req:log:export";   // {fmt: "csv"|"json"}

    // Boil Timer (app → device)
    constexpr const char* REQ_BOIL_START      = "req:boil:start";   // {min: uint16, additions: [{min, name}]}
    constexpr const char* REQ_BOIL_STOP       = "req:boil:stop";
    constexpr const char* REQ_BOIL_PAUSE      = "req:boil:pause";
    constexpr const char* REQ_BOIL_RESUME     = "req:boil:resume";
    constexpr const char* REQ_BOIL_ADD        = "req:boil:add";     // {min: uint16, name: string}

    // Mash-Out (app → device)
    constexpr const char* REQ_MASHOUT_SET     = "req:mashout:set";  // {enabled: bool, temp: float}

    // RTC (app → device)
    constexpr const char* REQ_RTC_GET         = "req:rtc:get";      // Get current RTC time
    constexpr const char* REQ_RTC_SET         = "req:rtc:set";      // {ts: unix_timestamp} or {iso: "YYYY-MM-DDTHH:MM:SS"}
    constexpr const char* REQ_RTC_SYNC        = "req:rtc:sync";     // Trigger NTP sync
    // P14 — timezone offset in minutes from UTC (e.g., -180 = UTC-3)
    constexpr const char* REQ_RTC_TZ_GET      = "req:rtc:tz:get";
    constexpr const char* REQ_RTC_TZ_SET      = "req:rtc:tz:set";   // {min: int16}

    // Timer (app → device)
    constexpr const char* REQ_TIMER_START     = "req:timer:start";  // {sec: uint32} or {min: uint32}
    constexpr const char* REQ_TIMER_ALARM     = "req:timer:alarm";  // {hour: uint8, min: uint8} absolute alarm
    constexpr const char* REQ_TIMER_STOP      = "req:timer:stop";
    constexpr const char* REQ_TIMER_PAUSE     = "req:timer:pause";
    constexpr const char* REQ_TIMER_RESUME    = "req:timer:resume";
    constexpr const char* REQ_TIMER_ADD       = "req:timer:add";    // {sec: int32} add/subtract time

    // Scheduler (app → device) - "be ready at HH:MM"
    constexpr const char* REQ_SCHEDULER_SET   = "req:sched:set";    // {hour, min, temp, vol}
    constexpr const char* REQ_SCHEDULER_STOP  = "req:sched:stop";

    // WiFi Events (device → app)
    constexpr const char* EVT_WIFI_STATUS     = "evt:wifi:status";

    // OTA Events (device → app)
    constexpr const char* EVT_OTA_STATUS      = "evt:ota:status";

    // Ramp Events (device → app)
    constexpr const char* EVT_RAMP_STATUS     = "evt:ramp:status";
    constexpr const char* EVT_RAMP_COMPLETE   = "evt:ramp:complete";

    // Brew Log Events (device → app)
    constexpr const char* EVT_LOG_STATUS      = "evt:log:status";
    constexpr const char* EVT_LOG_DATA        = "evt:log:data";     // Chunked export data

    // Boil Timer Events (device → app)
    constexpr const char* EVT_BOIL_STATUS     = "evt:boil:status";
    constexpr const char* EVT_BOIL_ADDITION   = "evt:boil:addition"; // Addition alert

    // RTC Events (device → app)
    constexpr const char* EVT_RTC_STATUS      = "evt:rtc:status";   // RTC status update

    // Timer Events (device → app)
    constexpr const char* EVT_TIMER_STATUS    = "evt:timer:status"; // Timer status update
    constexpr const char* EVT_TIMER_COMPLETE  = "evt:timer:complete"; // Timer finished

    // Scheduler Events (device → app)
    constexpr const char* EVT_SCHEDULER_STATUS  = "evt:sched:status";  // Scheduler status
    constexpr const char* EVT_SCHEDULER_STARTING = "evt:sched:starting"; // Heating started
    constexpr const char* EVT_SCHEDULER_READY   = "evt:sched:ready";   // Target reached

    // Auto-Tune Events (device → app)
    constexpr const char* EVT_AUTOTUNE_STATUS = "evt:autotune:status"; // Progress/status
    constexpr const char* EVT_AUTOTUNE_RESULT = "evt:autotune:result"; // Final Kp, Ki, Kd

    // Responses (device → app)
    constexpr const char* RES_OK            = "res:ok";
    constexpr const char* RES_ERROR         = "res:error";
    constexpr const char* RES_RECIPE_LIST   = "res:recipe:list";
    constexpr const char* RES_RECIPE_LOAD   = "res:recipe:load";
    constexpr const char* RES_SETTINGS      = "res:settings";
    constexpr const char* RES_SETTINGS_THERMAL = "res:settings:thermal";
    constexpr const char* RES_INFO          = "res:info";

    // ── JSON Field Names ────────────────────────────────────
    constexpr const char* FIELD_TYPE        = "tp";
    constexpr const char* FIELD_REQUEST_ID  = "rid";
    constexpr const char* FIELD_VALUE       = "v";
    constexpr const char* FIELD_ERROR       = "err";
    
    // Status fields
    constexpr const char* FIELD_CURRENT_TEMP = "ct";
    constexpr const char* FIELD_TARGET_TEMP  = "tt";
    constexpr const char* FIELD_PID_OUTPUT   = "out";
    constexpr const char* FIELD_MODE         = "m";
    constexpr const char* FIELD_HEATER_ON    = "h";
    constexpr const char* FIELD_PUMP_ON      = "p";
    constexpr const char* FIELD_RECIPE_STEP  = "rs";
    constexpr const char* FIELD_RECIPE_TOTAL = "rt";
    constexpr const char* FIELD_RECIPE_NAME  = "rn";
    constexpr const char* FIELD_TIMER_LEFT   = "tl";
    constexpr const char* FIELD_UPTIME       = "up";

    // Mode values
    constexpr const char* MODE_IDLE          = "idle";
    constexpr const char* MODE_MANUAL        = "manual";
    constexpr const char* MODE_RECIPE        = "recipe";
    constexpr const char* MODE_TUNING        = "tuning";

} // namespace Protocol
