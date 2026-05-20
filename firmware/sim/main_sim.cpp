// main_sim.cpp — entry point for the Inversa brewing simulator.
//
// Builds in env `sim`, links the real firmware plugins against the
// `sim/` HAL stubs, and drives a closed-loop ThermalSim while the
// firmware ticks. BLE I/O is replaced by stdin/stdout JSON.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "Arduino.h"
#include "SPI.h"
#include "SD.h"
#include "Wire.h"
#include "Preferences.h"
#include "WiFi.h"
#include "WiFiClientSecure.h"
#include "HTTPClient.h"
#include "Update.h"
#include "RTClib.h"
#include "NimBLEDevice.h"
#include "esp_ota_ops.h"
#include "SimArgs.h"
#include "SimClock.h"
#include "ThermalSim.h"

// Pin bindings consumed by sim/Arduino.h.
int simNtcPin  = -1;
int simSsrPin  = -1;
int simPumpPin = -1;

// Global instances expected by the firmware HAL stubs.
SimSerial   Serial;
ESPClass    ESP;
SPIClass    SPI;
SDClass     SD;
UpdateClass Update;
WiFiClass   WiFi;
TwoWire     Wire;

#include "core/constants.h"
#include "core/EventBus.h"
#include "core/PluginManager.h"
#include "core/NVSStorage.h"
#include "core/RecoveryManager.h"
#include "models/MachineState.h"

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
#include "plugins/BoilTimerPlugin.h"
#include "plugins/RTCPlugin.h"
#include "plugins/TimerPlugin.h"
#include "plugins/AutoTunePlugin.h"
#include "plugins/SchedulerPlugin.h"
#include "plugins/CommandHandler.h"

MachineState gState;
CommandHandler commandHandler;

namespace {

void onSigInt(int) { SimClock::requestExit(); }

std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::string();
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Background thread that pipes stdin JSON lines into the EventBus, mimicking
// the BLE write callback path on real hardware.
void stdinReaderLoop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        // Publish raw to the same event other plugins (CommandHandler) listen on.
        EventBus::instance().publish(EventType::BLECommandReceived, String(line.c_str()));
    }
}

}  // namespace

int main(int argc, char** argv) {
    SimArgs args;
    if (!parseSimArgs(argc, argv, args)) return 1;

    std::signal(SIGINT, onSigInt);

    // ── Configure ThermalSim from CLI ──────────────────────────────
    ThermalSim::configure({
        .volumeLiters       = args.volumeLiters,
        .heaterPowerW       = args.heaterPowerW,
        .efficiency         = 0.90f,
        .ambientTempC       = args.ambientC,
        .diameterM          = args.diameterM,
        .heatLossCoeffLidOn  = args.lossCoeff,
        .heatLossCoeffLidOff = args.lossCoeff,
        .lidOn               = true,
        .initialTempC       = args.ambientC,
    });

    // Bind GPIO pins the sim cares about. Values come from the production
    // board headers — they're just integers.
    simNtcPin  = PIN_NTC;
    simSsrPin  = PIN_HEATER_SSR;
    simPumpPin = PIN_PUMP_RELAY;

    // ── NVS pre-population (so plugins see persisted state) ────────
    NVSStorage::instance().begin();
    NVSStorage::instance().saveThermalParams(
        args.volumeLiters, args.heaterPowerW, args.ambientC,
        args.diameterM, args.lossCoeff, args.lossCoeff,
        /*ambientSource=*/0 /*MANUAL*/);

    // Mirror to gState so feed-forward in PID has values before NVS read.
    gState.volumeLiters         = args.volumeLiters;
    gState.heaterPowerWatts     = args.heaterPowerW;
    gState.ambientTemp          = args.ambientC;
    gState.vesselDiameter       = args.diameterM;
    gState.heatLossCoeffLidOn   = args.lossCoeff;
    gState.heatLossCoeffLidOff  = args.lossCoeff;
    gState.ambientSource        = AmbientSource::MANUAL;
    gState.lidState             = LidState::ON;

    // ── Register plugins ──────────────────────────────────────────
    auto& pm = PluginManager::instance();

    auto* temp      = pm.add<TemperaturePlugin>();
    auto* pid       = pm.add<PIDPlugin>();
    auto* heater    = pm.add<HeaterPlugin>();
    auto* pump      = pm.add<PumpPlugin>();
    auto* sd        = pm.add<SDCardPlugin>();
    auto* recipe    = pm.add<RecipePlugin>();
    auto* ble       = pm.add<BLEPlugin>();
    auto* wifi      = pm.add<WiFiPlugin>(gState);
    auto* ramp      = pm.add<RampPlugin>();
    auto* brewLog   = pm.add<BrewLogPlugin>();
    auto* boilTimer = pm.add<BoilTimerPlugin>();
    auto* rtc       = pm.add<RTCPlugin>(gState, *wifi);
    auto* timer     = pm.add<TimerPlugin>(gState, rtc);
    auto* autoTune  = pm.add<AutoTunePlugin>(gState, *pid);
    auto* scheduler = pm.add<SchedulerPlugin>(gState, *rtc);

    // Suppress unused-variable warnings on plugins we don't touch directly.
    (void)temp; (void)heater; (void)pump; (void)pid; (void)ramp;
    (void)brewLog; (void)boilTimer; (void)timer; (void)autoTune; (void)scheduler;

    pm.setup();

    brewLog->setSDCard(sd);
    RecoveryManager::instance().init(sd);

    // CommandHandler — pass nullptr for OTA since the sim doesn't include it.
    commandHandler.init(ble, sd, recipe, pid, wifi, /*ota*/ nullptr,
                        ramp, brewLog, boilTimer, rtc, timer, autoTune, scheduler);

    EventBus::instance().publish(EventType::SystemReady);

    // ── Optional auto-load recipe ──────────────────────────────────
    if (!args.recipePath.empty()) {
        auto content = slurp(args.recipePath);
        if (content.empty()) {
            std::fprintf(stderr, "[sim] failed to read recipe '%s'\n", args.recipePath.c_str());
            return 2;
        }
        // Filename in SD must end in .txt and pass the P2 validator. Strip
        // host directory.
        std::string base = args.recipePath;
        auto slash = base.find_last_of("/\\");
        if (slash != std::string::npos) base = base.substr(slash + 1);
        SD.simInject(std::string("/recipes/") + base, content);
        if (recipe->loadRecipe(String(content.c_str()), String(base.c_str()))) {
            recipe->start();
            std::fprintf(stderr, "[sim] auto-started recipe %s (%d steps)\n",
                         base.c_str(), recipe->getTotalSteps());
        } else {
            std::fprintf(stderr, "[sim] recipe failed to load\n");
            return 3;
        }
    }

    // Exit cleanly when the recipe completes (if --exit-on-complete).
    // We defer the exit by ~1 sim second so one more telemetry tick captures
    // the post-completion state (mode → idle, step at total). Without this,
    // CI assertions about "did the recipe finish" had to read the firmware's
    // stderr instead of just the JSONL stream.
    static std::atomic<int32_t> exitCountdownMs{-1};
    if (args.exitOnComplete) {
        EventBus::instance().subscribe(EventType::RecipeCompleted, [](const Event&) {
            std::fprintf(stderr, "[sim] RecipeCompleted — finishing telemetry tick\n");
            exitCountdownMs.store(1500);  // ~1.5 sim seconds to flush telemetry
        });
    }

    std::thread stdinReader(stdinReaderLoop);
    stdinReader.detach();

    // ── Main tick loop ─────────────────────────────────────────────
    constexpr uint32_t SIM_DT_MS = 100;
    const float SIM_DT_S = SIM_DT_MS / 1000.0f;
    const uint32_t maxSimMs = args.maxSimSec * 1000u;

    while (!SimClock::shouldExit()) {
        SimClock::advanceMs(SIM_DT_MS);
        ThermalSim::tick(SIM_DT_S);
        pm.loop();

        if (SimClock::nowMs() >= maxSimMs) {
            std::fprintf(stderr, "[sim] reached max sim time (%u s) — exiting\n", args.maxSimSec);
            break;
        }

        // Recipe completion countdown — let one more telemetry tick land.
        int32_t cd = exitCountdownMs.load();
        if (cd >= 0) {
            cd -= (int32_t)SIM_DT_MS;
            if (cd <= 0) SimClock::requestExit();
            else         exitCountdownMs.store(cd);
        }

        if (args.scale > 0.0f) {
            auto wallDtMs = (uint32_t)(SIM_DT_MS / args.scale);
            if (wallDtMs > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(wallDtMs));
            }
        }
    }

    std::fprintf(stderr, "[sim] shutdown at sim_t=%.1fs, T=%.2f°C\n",
                 SimClock::nowMs() / 1000.0f, ThermalSim::tempC());
    return 0;
}
