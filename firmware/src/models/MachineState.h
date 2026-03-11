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
};

// Global state — accessible by all plugins
extern MachineState gState;
