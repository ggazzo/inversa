// Inversa v2 — Homebrewing Temperature Controller
// main.cpp — Entry point

#include <Arduino.h>
#include "core/constants.h"
#include "core/EventBus.h"
#include "core/PluginManager.h"
#include "core/NVSStorage.h"
#include "core/RecoveryManager.h"
#include "models/MachineState.h"

// Plugins
#include "plugins/TemperaturePlugin.h"
#include "plugins/PIDPlugin.h"
#include "plugins/HeaterPlugin.h"
#include "plugins/PumpPlugin.h"
#include "plugins/SDCardPlugin.h"
#include "plugins/RecipePlugin.h"
#include "plugins/BLEPlugin.h"
#include "plugins/WiFiPlugin.h"
#include "plugins/OTAPlugin.h"
#include "plugins/RampPlugin.h"
#include "plugins/BrewLogPlugin.h"
#include "plugins/CommandHandler.h"

// ─── Global State ───────────────────────────────────────────
MachineState gState;

// ─── Command Handler (not a plugin, wired manually) ─────────
CommandHandler commandHandler;

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println();
    Serial.println("╔══════════════════════════════════════╗");
    Serial.println("║        Inversa v2 Brewing            ║");
    Serial.printf( "║  FW: %-32s║\n", BUILD_GIT_VERSION);
    Serial.println("╚══════════════════════════════════════╝");
    Serial.println();

    // Initialize NVS storage for persistent settings
    NVSStorage::instance().begin();

    auto& pm = PluginManager::instance();

    // Register plugins in dependency order
    auto* temp    = pm.add<TemperaturePlugin>();
    auto* pid     = pm.add<PIDPlugin>();
    auto* heater  = pm.add<HeaterPlugin>();
    auto* pump    = pm.add<PumpPlugin>();
    auto* sd      = pm.add<SDCardPlugin>();
    auto* recipe  = pm.add<RecipePlugin>();
    auto* ble     = pm.add<BLEPlugin>();
    auto* wifi    = pm.add<WiFiPlugin>(gState);
    auto* ota     = pm.add<OTAPlugin>(gState, *wifi);
    auto* ramp    = pm.add<RampPlugin>();
    auto* brewLog = pm.add<BrewLogPlugin>();

    // Initialize all plugins
    pm.setup();

    // Initialize recovery manager
    RecoveryManager::instance().init(sd);

    // Wire up command handler (routes BLE commands to plugins)
    commandHandler.init(ble, sd, recipe, pid, wifi, ota, ramp, brewLog);

    // Check for power loss recovery
    if (RecoveryManager::instance().hasValidRecovery()) {
        RecoveryData recoveryData;
        if (RecoveryManager::instance().loadRecovery(recoveryData)) {
            Serial.printf("[System] Recovery available: '%s' step %d/%d\n", 
                         recoveryData.recipeName, recoveryData.currentStep, recoveryData.totalSteps);
            gState.hasRecoveryData = true;
            gState.recoveryRecipeName = String(recoveryData.recipeName);
            // Notify app via BLE when client connects (handled in BLEPlugin)
        }
    }

    // System ready
    EventBus::instance().publish(EventType::SystemReady);
    
    Serial.println();
    Serial.printf("[System] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[System] Ready. Waiting for BLE connection...");
}

void loop() {
    PluginManager::instance().loop();
    gState.uptimeMs = millis();
}
