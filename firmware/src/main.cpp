// BrewPilot v3 — Homebrewing Temperature Controller
// main.cpp — Entry point

#include <Arduino.h>
#include <esp_ota_ops.h>           // P9: rollback automático pós-OTA
#include "core/constants.h"
#include "core/EventBus.h"
#include "core/PluginManager.h"
#include "core/NVSStorage.h"
#include "core/RecoveryManager.h"
#include "models/MachineState.h"

// Plugins
#include "plugins/TemperaturePlugin.h"
#include "plugins/PIDPlugin.h"
#include "core/IHeaterDriver.h"
#include "plugins/HeaterPlugin.h"
#if HEATER_DRIVER == HEATER_DRIVER_BURST_FIRE
#include "plugins/HeaterPluginZC.h"
#endif
#include "plugins/PumpPlugin.h"
#include "plugins/SDCardPlugin.h"
#include "plugins/RecipePlugin.h"
#include "plugins/BLEPlugin.h"
#include "plugins/WiFiPlugin.h"
#include "plugins/MDNSPlugin.h"
#include "plugins/OTAPlugin.h"
#include "plugins/RampPlugin.h"
#include "plugins/BrewLogPlugin.h"
#include "plugins/BoilTimerPlugin.h"
#include "plugins/RTCPlugin.h"
#include "plugins/TimerPlugin.h"
#include "plugins/AutoTunePlugin.h"
#include "plugins/SchedulerPlugin.h"
#include "plugins/ThermalWatchdogPlugin.h"
#include "plugins/AmbientSensorPlugin.h"
#include "plugins/LossTunePlugin.h"
#include "plugins/CommandHandler.h"
#ifdef DEV_OTA_ENABLED
#include "plugins/ArduinoOTAPlugin.h"
#endif
#ifdef HIL_BUILD
#include "plugins/HilHarnessPlugin.h"
#endif

// ─── Global State ───────────────────────────────────────────
MachineState gState;

// ─── Command Handler (not a plugin, wired manually) ─────────
CommandHandler commandHandler;

// ─── Watchdog handle (kicked from loop()) ───────────────────
static ThermalWatchdogPlugin* g_watchdog = nullptr;

void setup() {
    Serial.begin(115200);
    delay(500);
    
    DEBUG_PRINTLN();
    DEBUG_PRINTLN("╔══════════════════════════════════════╗");
    DEBUG_PRINTLN("║        BrewPilot v2 Brewing            ║");
    DEBUG_PRINTF( "║  FW: %-32s║\n", BUILD_GIT_VERSION);
    DEBUG_PRINTLN("╚══════════════════════════════════════╝");
    DEBUG_PRINTLN();

    // Initialize NVS storage for persistent settings
    NVSStorage::instance().begin();

    // P13: load persisted thermal params into gState before any plugin reads them
    // (PIDPlugin feed-forward depends on these). Defaults from MachineState are
    // used on fresh installs (hasThermalParams() == false).
    {
        auto& nvs = NVSStorage::instance();
        if (nvs.hasThermalParams()) {
            gState.volumeLiters     = nvs.loadThermalVolumeL(gState.volumeLiters);
            gState.heaterPowerWatts = nvs.loadThermalPowerW(gState.heaterPowerWatts);
            gState.ambientTemp      = nvs.loadThermalAmbientC(gState.ambientTemp);
            gState.vesselDiameter   = nvs.loadThermalDiameterM(gState.vesselDiameter);
            // Migrate legacy single-coeff key into dual-coeff slots if either is missing.
            nvs.migrateLegacyLossCoeff(gState.heatLossCoeffLidOn);
            gState.heatLossCoeffLidOn  = nvs.loadThermalLossCoeffLidOn (gState.heatLossCoeffLidOn);
            gState.heatLossCoeffLidOff = nvs.loadThermalLossCoeffLidOff(gState.heatLossCoeffLidOff);
            gState.ambientSource = (AmbientSource)nvs.loadThermalAmbientSource((uint8_t)gState.ambientSource);
            DEBUG_PRINTF("[System] Thermal params loaded: V=%.1fL P=%.0fW Tamb=%.1f D=%.2fm h_on=%.1f h_off=%.1f src=%u\n",
                         gState.volumeLiters, gState.heaterPowerWatts, gState.ambientTemp,
                         gState.vesselDiameter, gState.heatLossCoeffLidOn,
                         gState.heatLossCoeffLidOff, (unsigned)gState.ambientSource);
        }
        if (nvs.hasTempCalibration()) {
            gState.tempCalSlope  = nvs.loadTempCalSlope(gState.tempCalSlope);
            gState.tempCalOffset = nvs.loadTempCalOffset(gState.tempCalOffset);
            DEBUG_PRINTF("[System] Temp calibration loaded: slope=%.4f offset=%.2f\n",
                         gState.tempCalSlope, gState.tempCalOffset);
        }
    }

    auto& pm = PluginManager::instance();

    // Register plugins in dependency order. ThermalWatchdog is placed
    // AFTER HeaterPlugin so HeaterPlugin::setup() has configured the
    // SSR pin as OUTPUT before Watchdog::setup() exercises it. Run-
    // order trade-off (Heater may write HIGH then Watchdog overrides
    // LOW in the same iteration on a fresh trip) is mitigated by the
    // direct gpio_set_level inside Watchdog::trip().
    auto* temp     = pm.add<TemperaturePlugin>();
    auto* pid      = pm.add<PIDPlugin>();
#if HEATER_DRIVER == HEATER_DRIVER_BURST_FIRE
    uint16_t zcMainsFreqHz = NVSStorage::instance().loadHeaterMainsFreqHz((uint16_t)MAINS_FREQ_HZ);
    uint8_t  zcBurstWindow = NVSStorage::instance().loadHeaterBurstWindow((uint8_t)HEATER_BURST_WINDOW);
    IHeaterDriver* heater = pm.add<HeaterPluginZC>(
        (uint8_t)PIN_HEATER_SSR,
        (uint8_t)PIN_HEATER_ZC,
        zcMainsFreqHz,
        zcBurstWindow);
#else
    IHeaterDriver* heater = pm.add<HeaterPlugin>();
#endif
    (void)heater;
    auto* watchdog = pm.add<ThermalWatchdogPlugin>();
    g_watchdog     = watchdog;
    auto* pump     = pm.add<PumpPlugin>();
    auto* sd       = pm.add<SDCardPlugin>();
    auto* recipe   = pm.add<RecipePlugin>();
    auto* ble      = pm.add<BLEPlugin>();
    auto* wifi     = pm.add<WiFiPlugin>(gState);
    pm.add<MDNSPlugin>();
    auto* ota      = pm.add<OTAPlugin>(gState, *wifi);
    auto* ramp      = pm.add<RampPlugin>();
    auto* brewLog   = pm.add<BrewLogPlugin>();
    auto* boilTimer = pm.add<BoilTimerPlugin>();
    auto* rtc       = pm.add<RTCPlugin>(gState, *wifi);
    auto* timer     = pm.add<TimerPlugin>(gState, rtc);
    auto* autoTune  = pm.add<AutoTunePlugin>(gState, *pid);
    auto* scheduler = pm.add<SchedulerPlugin>(gState, *rtc);
    auto* ambient   = pm.add<AmbientSensorPlugin>();
    (void)ambient;
    auto* lossTune  = pm.add<LossTunePlugin>();
#ifdef DEV_OTA_ENABLED
    pm.add<ArduinoOTAPlugin>();
#endif

#ifdef HIL_BUILD
    // HIL harness — JSONL command channel for the host bridge. Registered
    // last so all the plugins it can poke (Watchdog, Recipe, etc.) are
    // already in the manager's vector. setup() runs the same way as any
    // other plugin.
    auto* hil = pm.add<HilHarnessPlugin>(watchdog, recipe);
    (void)hil;
#endif

    // Initialize all plugins
    pm.setup();

    // P18 — give BrewLog a handle to SD for batched flushes.
    brewLog->setSDCard(sd);

    // Initialize recovery manager
    RecoveryManager::instance().init(sd);
    // P11 — increment bootAttempts BEFORE checking validity, so repeated
    // crashes during restore eventually exceed the limit and we stop offering.
    RecoveryManager::instance().noteBootAttempt();

    // Wire up command handler (routes BLE commands to plugins)
    commandHandler.init(ble, sd, recipe, pid, wifi, ota, ramp, brewLog, boilTimer, rtc, timer, autoTune, scheduler, watchdog, lossTune, heater);

    // Check for power loss recovery
    if (RecoveryManager::instance().hasValidRecovery()) {
        RecoveryData recoveryData;
        if (RecoveryManager::instance().loadRecovery(recoveryData)) {
            DEBUG_PRINTF("[System] Recovery available: '%s' step %d/%d\n", 
                         recoveryData.recipeName, recoveryData.currentStep, recoveryData.totalSteps);
            gState.hasRecoveryData = true;
            setStr(gState.recoveryRecipeName, recoveryData.recipeName);
            // Notify app via BLE when client connects (handled in BLEPlugin)
        }
    }

    // System ready
    EventBus::instance().publish(EventType::SystemReady);

    DEBUG_PRINTLN();
    DEBUG_PRINTF("[System] Free heap: %d bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("[System] Ready. Waiting for BLE connection...");

    // ─── P9: OTA rollback automático ────────────────────────────
    // Se este boot é um firmware recém-instalado via OTA, o ESP-IDF marca
    // a partição como ESP_OTA_IMG_PENDING_VERIFY. Damos 60s de operação
    // estável antes de marcar como VALID. Se o firmware crashar antes
    // disso, o bootloader faz rollback automático para a versão anterior
    // no próximo boot.
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t otaState;
    if (running && esp_ota_get_state_partition(running, &otaState) == ESP_OK) {
        if (otaState == ESP_OTA_IMG_PENDING_VERIFY) {
            gState.otaVerifyDeadline = millis() + 60000;  // 60s
            DEBUG_PRINTLN("[OTA] Firmware pending verify — 60s para confirmar estabilidade");
        }
    }
}

void loop() {
    PluginManager::instance().loop();
    gState.uptimeMs = millis();

    // Thermal Watchdog heartbeat (001-thermal-watchdog T026).
    // The esp_timer task reads this counter every WATCHDOG_ISR_INTERVAL_US
    // and trips LOOP_STUCK if the gap exceeds watchdogLoopStuckMs.
    if (g_watchdog) g_watchdog->kick();

    // ─── P9: confirmar firmware estável após 60s sem crash ──────
    if (gState.otaVerifyDeadline > 0 && millis() > gState.otaVerifyDeadline) {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            DEBUG_PRINTLN("[OTA] Firmware marcado como VALID — rollback desabilitado");
        } else {
            DEBUG_PRINTF("[OTA] Falha ao marcar app valid (err=%d)\n", err);
        }
        gState.otaVerifyDeadline = 0;
    }
}
