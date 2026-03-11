#pragma once
#include <Arduino.h>

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
    String recipeName       = "";
    int    recipeStep       = 0;
    int    recipeTotalSteps = 0;
    uint32_t timerRemainingMs = 0;
    String confirmMessage   = "";

    // BLE
    bool  bleConnected      = false;

    // System
    uint32_t uptimeMs       = 0;

    // Recovery
    bool   hasRecoveryData      = false;
    String recoveryRecipeName   = "";

    // WiFi
    bool   wifiConnected        = false;
    String wifiSSID             = "";
    String wifiIP               = "";
    String wifiConfiguredSSID   = "";  // Stored SSID (for display)

    // OTA
    String otaStatus            = "idle";  // idle, checking, available, downloading, installing, error, up-to-date
    String otaLatestVersion     = "";
    uint8_t otaProgress         = 0;
    String otaError             = "";

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

    // Auto-Tune
    bool     autoTuneActive     = false;
    uint8_t  autoTuneProgress   = 0;      // Progress percentage (0-100)
    String   autoTuneStatus     = "";     // Status message
};

// Global state — accessible by all plugins
extern MachineState gState;
