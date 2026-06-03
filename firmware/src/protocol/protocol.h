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
    // Temperature calibration — linear correction { slope, offset }.
    constexpr const char* REQ_SETTINGS_CAL_GET     = "req:settings:cal:get";
    constexpr const char* REQ_SETTINGS_CAL_SET     = "req:settings:cal:set";
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
    // Install a specific build chosen from the catalog (not just latest).
    // { "url": "<bin>", "sig": "<bin.sig>", "ver": "<label>" }. The ECDSA
    // signature is still verified against the embedded pubkey, so an arbitrary
    // URL is safe; the semver "is newer" gate is skipped for an explicit pick.
    constexpr const char* REQ_OTA_INSTALL_BUILD = "req:ota:install-build";

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

    // Thermal Watchdog (app → device) — 001-thermal-watchdog
    constexpr const char* REQ_WATCHDOG_RESET  = "req:watchdog:reset";   // no args
    constexpr const char* REQ_WATCHDOG_CONFIG = "req:watchdog:config";  // partial config object

    // Heater driver config (burst-fire / zero-cross) — partial update.
    // { "freq": 50|60, "burst_window": uint8 }. Persisted to NVS.
    // burst_window applies at runtime; freq requires reboot.
    constexpr const char* REQ_HEATER_CONFIG   = "req:heater:config";

    // Device identity. { "name": "<label>" } sets the user-visible BLE
    // advertisement name and mDNS hostname; persists to NVS and reboots
    // so the new name takes effect on the next adv frame.
    constexpr const char* REQ_DEVICE_RENAME   = "req:device:rename";

    // Dev push-OTA (ArduinoOTA/espota) config. { "pwd": "<password>" } sets the
    // per-device upload password in NVS and (re)opens the listener; an empty
    // pwd closes it. Only has effect on DEV_OTA_ENABLED builds — keeps the
    // secret out of the published binary (set it once over BLE instead).
    constexpr const char* REQ_DEVOTA_CONFIG   = "req:devota:config";

    // LossTune — auto-tune of heat-loss coefficient
    constexpr const char* REQ_LOSSTUNE_START  = "req:losstune:start";   // {mode:"lidOn"|"lidOff"}
    constexpr const char* REQ_LOSSTUNE_CANCEL = "req:losstune:cancel";
    constexpr const char* REQ_LOSSTUNE_ACCEPT = "req:losstune:accept";  // persist fitted coeff
    constexpr const char* REQ_LOSSTUNE_REJECT = "req:losstune:reject";  // discard fit, back to IDLE

    // Lid state runtime selector — picks which lossCoeff_* drives PID/Scheduler
    constexpr const char* REQ_LID_STATE_SET   = "req:lid:set";          // {mode:"lidOn"|"lidOff"}

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

    // Thermal Watchdog Events (device → app) — 001-thermal-watchdog
    constexpr const char* EVT_WATCHDOG_TRIPPED = "evt:watchdog:tripped"; // {cause, temp, unix}
    constexpr const char* EVT_WATCHDOG_RESET   = "evt:watchdog:reset";   // {auto, prev_cause}

    // LossTune Events (device → app)
    constexpr const char* EVT_LOSSTUNE_STATUS  = "evt:losstune:status";  // phase + progress + R²
    constexpr const char* EVT_LOSSTUNE_SAMPLE  = "evt:losstune:sample";  // batched (t,T) during DECAY
    constexpr const char* EVT_LOSSTUNE_RESULT  = "evt:losstune:result";  // fit done, awaiting accept

    // Responses (device → app)
    constexpr const char* RES_OK            = "res:ok";
    constexpr const char* RES_ERROR         = "res:error";
    constexpr const char* RES_RECIPE_LIST   = "res:recipe:list";
    constexpr const char* RES_RECIPE_LOAD   = "res:recipe:load";
    constexpr const char* RES_SETTINGS      = "res:settings";
    constexpr const char* RES_SETTINGS_THERMAL = "res:settings:thermal";
    constexpr const char* RES_SETTINGS_CAL     = "res:settings:cal";
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

    // Thermal Watchdog telemetry — nested object key + subfields
    constexpr const char* FIELD_WATCHDOG     = "wd";
    constexpr const char* WD_FIELD_ARMED       = "a";
    constexpr const char* WD_FIELD_TRIPPED     = "t";
    constexpr const char* WD_FIELD_COUNT       = "c";
    constexpr const char* WD_FIELD_LAST_CAUSE  = "lc";
    constexpr const char* WD_FIELD_LAST_UNIX   = "lu";
    constexpr const char* WD_FIELD_HARD_STOP   = "hs";
    constexpr const char* WD_FIELD_AUTO_RESET  = "ar";
    // Full config — exposed so the UI can render every editable field
    // (T032 ext). Keys mirror the JSON accepted by req:watchdog:config.
    constexpr const char* WD_FIELD_SENSOR_FAULT_MS  = "sfm";
    constexpr const char* WD_FIELD_LOOP_STUCK_MS    = "lsm";
    constexpr const char* WD_FIELD_GRAD_FACTOR      = "gf";
    constexpr const char* WD_FIELD_GRAD_WINDOW      = "gw";
    constexpr const char* WD_FIELD_SAFE_AUTORESET_C = "sa";
    constexpr const char* WD_FIELD_COOL_MIN_MS      = "cm";

    // Mode values
    constexpr const char* MODE_IDLE          = "idle";
    constexpr const char* MODE_MANUAL        = "manual";
    constexpr const char* MODE_RECIPE        = "recipe";
    constexpr const char* MODE_TUNING        = "tuning";

} // namespace Protocol
