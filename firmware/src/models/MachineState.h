#pragma once
#include <Arduino.h>
#include <cstring>

// Bounded copy helper used by the char[] string fields below. Returns a
// reference to dst so it composes with operator chains. Truncates to N-1
// chars and always NUL-terminates — caller does not need to handle the
// '\0' itself. Empty source clears the buffer.
template <size_t N>
inline void setStr(char (&dst)[N], const char* src) {
    if (!src) { dst[0] = 0; return; }
    size_t i = 0;
    for (; i < N - 1 && src[i] != 0; i++) dst[i] = src[i];
    dst[i] = 0;
}
template <size_t N>
inline void setStr(char (&dst)[N], const String& src) { setStr(dst, src.c_str()); }
template <size_t N>
inline bool isEmptyStr(const char (&buf)[N]) { return buf[0] == 0; }

// ─── Operating Mode ─────────────────────────────────────────
enum class OperatingMode : uint8_t {
    Idle,
    Manual,
    Recipe,
    Tuning
};

// ─── Recipe State ───────────────────────────────────────────
enum class RecipeState : uint8_t {
    Idle,
    Running,
    Paused,
    WaitingForTemperature,
    WaitingForTimer,
    WaitingForConfirm,
    Preparing,       // pre-heating
    Completed
};

// ─── Brewing Step (for UI visualization) ────────────────────
// Represents the current phase in the brewing process
enum class BrewingStep : uint8_t {
    None        = 0,    // No active step
    PreHeating  = 1,    // Pre-heating water
    Mashing     = 2,    // Mashing (starch conversion)
    MashOut     = 3,    // Mash-out (stop enzyme activity)
    Sparge      = 4,    // Sparging (grain rinse)
    Boiling     = 5,    // Boiling wort
    Hopping     = 6,    // Adding hops
    Cooling     = 7,    // Cooling wort
    Done        = 8     // Process complete
};

// Step names for display (Portuguese)
inline const char* getStepName(BrewingStep step) {
    switch (step) {
        case BrewingStep::PreHeating: return "Pre-aquecimento";
        case BrewingStep::Mashing:    return "Mostura";
        case BrewingStep::MashOut:    return "Mash-out";
        case BrewingStep::Sparge:     return "Lavagem";
        case BrewingStep::Boiling:    return "Fervura";
        case BrewingStep::Hopping:    return "Lupulagem";
        case BrewingStep::Cooling:    return "Resfriamento";
        case BrewingStep::Done:       return "Concluido";
        default:                      return "";
    }
}

// ─── Machine State ──────────────────────────────────────────
struct MachineState {
    // Temperature
    float currentTemp       = 0.0f;
    float targetTemp        = 0.0f;
    bool  tempSensorOk      = false;

    // PID
    float pidOutput         = 0.0f;
    float pidKp             = 0.0f;
    float pidKi             = 0.0f;
    float pidKd             = 0.0f;

    // Actuators
    bool  heaterOn          = false;
    bool  pumpOn            = false;

    // Mode
    OperatingMode mode      = OperatingMode::Idle;

    // Recipe
    RecipeState recipeState = RecipeState::Idle;
    char   recipeName[32]   = {0};
    int    recipeStep       = 0;
    int    recipeTotalSteps = 0;
    uint32_t timerRemainingMs = 0;
    char   confirmMessage[64] = {0};
    // P5 — sub-states the RecipePlugin can be sitting in, captured by
    // RecoveryManager so a power-cut resumes the correct WAIT_*.
    bool   waitingForTemp    = false;
    bool   waitingForTimer   = false;
    bool   waitingForBoil    = false;
    bool   waitingForRamp    = false;
    bool   waitingForConfirm = false;
    // P5 — paused duration accumulator (substracted from elapsed time in
    // timer-based WAIT_TIME so a pause doesn't shorten the wait).
    uint32_t recipePausedDurationMs = 0;

    // BLE
    bool  bleConnected      = false;

    // System
    uint32_t uptimeMs       = 0;

    // Recovery
    bool   hasRecoveryData      = false;
    char   recoveryRecipeName[32] = {0};

    // WiFi
    bool   wifiConnected        = false;
    char   wifiSSID[33]         = {0};
    char   wifiIP[16]           = {0};
    char   wifiConfiguredSSID[33] = {0};  // Stored SSID (for display)

    // OTA
    char   otaStatus[16]        = "idle";  // idle, checking, available, downloading, installing, error, up-to-date
    char   otaLatestVersion[32] = {0};
    uint8_t otaProgress         = 0;
    char   otaError[64]         = {0};
    // P9 fix: rollback automático pós-OTA
    // Quando != 0, indica que firmware atual está em ESP_OTA_IMG_PENDING_VERIFY.
    // Ao expirar (millis() >= otaVerifyDeadline), main loop chama
    // esp_ota_mark_app_valid_cancel_rollback() para confirmar boot estável.
    uint32_t otaVerifyDeadline  = 0;

    // Ramp Mode
    bool   rampActive           = false;
    float  rampTarget           = 0.0f;
    float  rampCurrent          = 0.0f;
    float  rampRate             = 0.0f;   // °C/min

    // Brew Log
    bool   brewLogActive        = false;
    uint32_t brewLogStartTime   = 0;
    uint16_t brewLogEntries     = 0;

    // Boil Timer
    bool     boilActive         = false;
    bool     boilPaused         = false;   // P5 — for recovery
    uint32_t boilTotal          = 0;      // Total boil time in seconds
    uint32_t boilRemaining      = 0;      // Remaining time in seconds
    uint8_t  boilAdditions      = 0;      // Number of additions configured

    // Mash-Out
    bool     mashOutEnabled     = false;
    float    mashOutTemp        = 76.0f;  // Default mash-out temperature

    // RTC
    bool     rtcAvailable       = false;
    uint32_t rtcTimestamp       = 0;      // Current unix timestamp
    bool     rtcNtpSynced       = false;

    // Timer (generic countdown timer, independent from recipe timer)
    bool     timerActive        = false;
    bool     timerPaused        = false;
    uint32_t timerTotal         = 0;      // Total duration in seconds
    uint32_t timerRemaining     = 0;      // Remaining time in seconds
    uint8_t  timerMode          = 0;      // 0=Relative, 1=Absolute
    uint8_t  timerAlarmHour     = 0;      // Alarm hour (0-23) for absolute mode
    uint8_t  timerAlarmMinute   = 0;      // Alarm minute (0-59) for absolute mode

    // Scheduler ("be ready at HH:MM")
    bool     schedulerActive    = false;
    uint8_t  schedulerTargetHour   = 0;   // Target ready time hour
    uint8_t  schedulerTargetMinute = 0;   // Target ready time minute
    float    schedulerTargetTemp   = 0;   // Target temperature
    float    schedulerVolume       = 0;   // Water volume in liters
    uint32_t schedulerStartTime    = 0;   // Calculated start unix timestamp
    char     schedulerStatus[32]   = {0}; // Status message

    // Auto-Tune
    bool     autoTuneActive     = false;
    uint8_t  autoTuneProgress   = 0;      // Progress percentage (0-100)
    char     autoTuneStatus[32] = {0};    // Status message

    // Brewing Step (for UI visualization)
    BrewingStep brewingStep     = BrewingStep::None;
    char     brewingStepCustom[32] = {0}; // Custom step name (from STEP command)

    // Thermal parameters (for heat loss calculation)
    float    volumeLiters       = 20.0f;  // Water/wort volume
    float    heaterPowerWatts   = 3000.0f; // Heater power
    float    ambientTemp        = 25.0f;  // Ambient temperature
    float    vesselDiameter     = 0.35f;  // Vessel diameter in meters (~35cm)
    float    heatLossCoeff      = 10.0f;  // Heat transfer coefficient (W/m²K)

    // Temperature calibration: T_real = slope · T_medido + offset.
    // Default is identity (no correction). Set via req:settings:cal:set,
    // applied in TemperaturePlugin after the Kalman filter, persisted in
    // NVS by NVSStorage::saveTempCalibration.
    float    tempCalSlope       = 1.0f;
    float    tempCalOffset      = 0.0f;
};

// Global state — accessible by all plugins
extern MachineState gState;
