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
    constexpr const char* EVT_RECIPE_STEP   = "evt:recipe:step";
    constexpr const char* EVT_RECIPE_STATE  = "evt:recipe:state";
    constexpr const char* EVT_RECIPE_CONFIRM = "evt:recipe:confirm";
    constexpr const char* EVT_ERROR         = "evt:error";
    constexpr const char* EVT_LOG           = "evt:log";

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
    constexpr const char* REQ_STATUS        = "req:status";
    constexpr const char* REQ_INFO          = "req:info";
    constexpr const char* REQ_AUTOTUNE_START = "req:autotune:start";
    constexpr const char* REQ_AUTOTUNE_STOP  = "req:autotune:stop";

    // Responses (device → app)
    constexpr const char* RES_OK            = "res:ok";
    constexpr const char* RES_ERROR         = "res:error";
    constexpr const char* RES_RECIPE_LIST   = "res:recipe:list";
    constexpr const char* RES_RECIPE_LOAD   = "res:recipe:load";
    constexpr const char* RES_SETTINGS      = "res:settings";
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
