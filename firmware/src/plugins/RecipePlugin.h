#pragma once

#include <Arduino.h>
#include <vector>
#include <cstring>
#include <cstdlib>
#include "../core/Plugin.h"
#include "../core/constants.h"
#include "../core/RecoveryManager.h"
#include "../models/MachineState.h"
#include "../core/RecipeParser.h"   // RecipeCommandType, RecipeCommand, parseLine

// ─── Recipe Plugin ──────────────────────────────────────────
// Extended recipe engine with full automation support.
// Commands are executed via EventBus to maintain loose coupling.

class RecipePlugin : public Plugin {
public:
    const char* getName() const override { return "Recipe"; }

    bool setup() override {
        // Listen for recipe confirm from user
        bus().subscribe(EventType::RecipeConfirmed, [this](const Event&) {
            if (gState.recipeState == RecipeState::WaitingForConfirm) {
                advanceStep();
            }
        });

        // Listen for temperature readings (for WAIT_TEMP)
        bus().subscribe(EventType::TemperatureRead, [this](const Event& e) {
            _currentTemp = e.floatValue;
        });

        // Listen for boil timer completion
        bus().subscribe(EventType::BoilCompleted, [this](const Event&) {
            if (_waitingForBoil) {
                _waitingForBoil = false;
                gState.waitingForBoil = false;  // P5
                advanceStep();
            }
        });

        // Listen for timer completion (TimerPlugin)
        bus().subscribe(EventType::TimerCompleted, [this](const Event&) {
            if (_waitingForTimer) {
                _waitingForTimer = false;
                gState.waitingForTimer = false;  // P5
                advanceStep();
            }
        });

        // Listen for ramp completion
        bus().subscribe(EventType::RampCompleted, [this](const Event&) {
            if (_waitingForRamp) {
                _waitingForRamp = false;
                gState.waitingForRamp = false;  // P5
                advanceStep();
            }
        });

        DEBUG_PRINTLN("[Recipe] Engine initialized (extended commands)");
        return true;
    }

    void loop() override {
        if (gState.recipeState == RecipeState::Idle ||
            gState.recipeState == RecipeState::Completed ||
            gState.recipeState == RecipeState::Paused) {
            return;
        }

        // Periodic save for power loss recovery
        RecoveryManager::instance().periodicSave();

        switch (gState.recipeState) {
            case RecipeState::Running:
                executeCurrentStep();
                break;

            case RecipeState::WaitingForTemperature:
                checkTemperatureReached();
                break;

            case RecipeState::WaitingForTimer:
                checkTimerExpired();
                break;

            case RecipeState::WaitingForConfirm:
                // Waiting for user confirmation via BLE
                break;

            case RecipeState::Preparing:
                // Waiting for external event (boil, timer, ramp)
                break;

            default:
                break;
        }
    }

    // ── Public API ──────────────────────────────────────────

    bool loadRecipe(const String& content, const String& name) {
        _commands.clear();
        _currentStep = 0;
        _hopAdditions.clear();

        // Walk content line-by-line without String::substring/trim — those
        // were allocating a fresh heap String per line, peaking heap usage
        // at recipe load. Now we just window into the existing buffer and
        // hand parseLine a NUL-terminated slice from a fixed scratch buffer.
        const char* base = content.c_str();
        size_t len = content.length();
        char line[128];
        size_t i = 0;
        while (i < len) {
            size_t j = i;
            while (j < len && base[j] != '\n') j++;
            // Strip leading/trailing whitespace from [i, j).
            size_t a = i;
            while (a < j && (base[a] == ' ' || base[a] == '\t' || base[a] == '\r')) a++;
            size_t b = j;
            while (b > a && (base[b-1] == ' ' || base[b-1] == '\t' || base[b-1] == '\r')) b--;
            size_t n = b - a;
            if (n > 0) {
                if (n >= sizeof(line)) n = sizeof(line) - 1;
                memcpy(line, base + a, n);
                line[n] = 0;
                RecipeCommand cmd = RecipeParser::parseLine(line, _waitTempTolerance);
                if (cmd.type != RecipeCommandType::Comment &&
                    cmd.type != RecipeCommandType::Unknown) {
                    _commands.push_back(cmd);
                }
            }
            i = j + 1;
        }

        setStr(gState.recipeName, name);
        gState.recipeTotalSteps = _commands.size();
        gState.recipeStep = 0;

        DEBUG_PRINTF("[Recipe] Loaded '%s' with %d steps\n",
                      name.c_str(), _commands.size());
        return !_commands.empty();
    }

    void start() {
        if (_commands.empty()) return;
        _currentStep = 0;
        _waitingForBoil = false;
        _waitingForTimer = false;
        _waitingForRamp = false;
        _pauseStartMs = 0;
        _hopAdditions.clear();
        clearWaitFlags();
        gState.recipePausedDurationMs = 0;

        gState.recipeState = RecipeState::Running;
        gState.recipeStep = 0;
        gState.mode = OperatingMode::Recipe;
        bus().publish(EventType::RecipeStarted, gState.recipeName);
        bus().publish(EventType::RecipeStepChanged, 0);
        sendRecipeStateBle("started");
        sendRecipeStepBle();
        DEBUG_PRINTLN("[Recipe] Started");
    }

    void stop() {
        gState.recipeState = RecipeState::Idle;
        gState.mode = OperatingMode::Idle;
        gState.targetTemp = 0;
        gState.timerRemainingMs = 0;
        _waitingForBoil = false;
        _waitingForTimer = false;
        _waitingForRamp = false;
        _pauseStartMs = 0;
        clearWaitFlags();
        gState.recipePausedDurationMs = 0;

        bus().publish(EventType::RecipeStopped);
        bus().publish(EventType::SetpointChanged, 0.0f);
        bus().publish(EventType::HeaterStateChanged, false);
        sendRecipeStateBle("stopped");
        RecoveryManager::instance().clearRecovery();
        DEBUG_PRINTLN("[Recipe] Stopped");
    }

    void pause() {
        if (gState.recipeState != RecipeState::Idle &&
            gState.recipeState != RecipeState::Completed &&
            gState.recipeState != RecipeState::Paused) {
            _pausedState = gState.recipeState;
            gState.recipeState = RecipeState::Paused;
            // P3 — capture pause start so the internal WAIT_TIMER doesn't drift.
            _pauseStartMs = millis();
            bus().publish(EventType::RecipePaused);
            sendRecipeStateBle("paused");
            DEBUG_PRINTLN("[Recipe] Paused");
        }
    }

    void resume() {
        if (gState.recipeState == RecipeState::Paused) {
            // P3 — shift _timerStart forward by the paused interval so the
            // WAIT_TIMER countdown sees the same effective elapsed time as
            // before the pause. Also surface the accumulator in gState so
            // RecoveryManager can snapshot it.
            uint32_t pausedFor = millis() - _pauseStartMs;
            _timerStart += pausedFor;
            gState.recipePausedDurationMs += pausedFor;
            _pauseStartMs = 0;
            gState.recipeState = _pausedState;
            bus().publish(EventType::RecipeResumed);
            sendRecipeStateBle("resumed");
            DEBUG_PRINTLN("[Recipe] Resumed");
        }
    }

    void confirm() {
        bus().publish(EventType::RecipeConfirmed);
    }

    int getCurrentStep() const { return _currentStep; }
    int getTotalSteps() const { return _commands.size(); }
    const std::vector<RecipeCommand>& getCommands() const { return _commands; }

    // Restore from recovery data (Recovery v2).
    //
    // P5 — reconciles _hopAdditions by walking commands [0.._currentStep):
    //   - AddHop      → push onto _hopAdditions
    //   - Boil        → clear _hopAdditions (those hops already went into the
    //                   previous BOIL invocation)
    // After the walk, _hopAdditions has exactly the hops collected after the
    // most recent BOIL — matching what a fresh execution from step 0 would
    // have at this point.
    //
    // The recovery file does NOT store the hop list itself; the recipe file
    // is the source of truth. This keeps RecoveryData small and avoids drift
    // between the file and the loaded recipe.
    bool restoreFromRecovery(const RecoveryData& data, const String& recipeContent) {
        if (!loadRecipe(recipeContent, String(data.recipeName))) {
            return false;
        }

        _currentStep            = data.currentStep;
        gState.recipeStep       = data.currentStep;
        gState.targetTemp       = data.targetTemp;
        gState.recipeState      = static_cast<RecipeState>(data.recipeState);
        gState.timerRemainingMs = data.timerRemainingMs;
        gState.mode             = OperatingMode::Recipe;
        gState.brewingStep      = static_cast<BrewingStep>(data.brewingStep);
        gState.mashOutEnabled   = data.mashOutEnabled;
        gState.mashOutTemp      = data.mashOutTemp;
        gState.recipePausedDurationMs = data.recipePausedDurationMs;

        // P5 — restore wait-state flags into RAM and gState.
        gState.waitingForTemp    = data.waitingForTemp;
        gState.waitingForTimer   = data.waitingForTimer;
        gState.waitingForBoil    = data.waitingForBoil;
        gState.waitingForRamp    = data.waitingForRamp;
        gState.waitingForConfirm = data.waitingForConfirm;
        _waitingForBoil  = data.waitingForBoil;
        _waitingForTimer = data.waitingForTimer;
        _waitingForRamp  = data.waitingForRamp;

        // P5 — reconcile _hopAdditions from _commands.
        _hopAdditions.clear();
        for (int i = 0; i < _currentStep && i < (int)_commands.size(); i++) {
            const RecipeCommand& c = _commands[i];
            if (c.type == RecipeCommandType::AddHop) {
                _hopAdditions.push_back(c);
            } else if (c.type == RecipeCommandType::Boil) {
                _hopAdditions.clear();  // hops consumed by this BOIL
            }
        }
        DEBUG_PRINTF("[Recipe] Reconciled %u pending hop additions\n",
                     (unsigned)_hopAdditions.size());

        // P11 — preserve the bootAttempts counter across periodic saves so
        // repeated restore crashes eventually exceed the limit.
        RecoveryManager::instance().setLiveBootAttempts(data.bootAttempts);

        if (gState.recipeState == RecipeState::WaitingForTimer && data.timerRemainingMs > 0) {
            _timerDuration = data.timerRemainingMs;
            _timerStart    = millis();
        }

        DEBUG_PRINTF("[Recipe] Restored: step %d, state %d, target %.1f, paused %ums\n",
                     _currentStep, data.recipeState, data.targetTemp,
                     (unsigned)data.recipePausedDurationMs);

        bus().publish(EventType::SetpointChanged, data.targetTemp);
        if (data.targetTemp > 0) {
            bus().publish(EventType::HeaterStateChanged, true);
        }

        return true;
    }

private:
    // BLE notifies for recipe lifecycle. App receives these as immediate
    // notify() messages on the Nordic UART characteristic, without
    // having to wait for the 1 Hz `evt:status` poll. The shape mirrors
    // BoilTimer's `evt:boil:*` so the app can use the same JSON router.
    void sendRecipeStateBle(const char* st) {
        String json = "{\"tp\":\"evt:recipe:state\",\"st\":\"";
        json += st;
        json += "\",\"name\":\"";
        json += (const char*)gState.recipeName;
        json += "\"}";
        bus().publish(EventType::BLESend, json);
    }
    void sendRecipeStepBle() {
        String json = "{\"tp\":\"evt:recipe:step\",\"step\":";
        json += String(_currentStep);
        json += ",\"total\":";
        json += String((int)_commands.size());
        // brewingStepCustom holds the human-readable label set by STEP "name".
        json += ",\"name\":\"";
        json += (const char*)gState.brewingStepCustom;
        json += "\"}";
        bus().publish(EventType::BLESend, json);
    }
    void sendRecipeWaitBle(const char* msg) {
        String json = "{\"tp\":\"evt:recipe:wait\",\"msg\":\"";
        json += msg ? msg : "";
        json += "\"}";
        bus().publish(EventType::BLESend, json);
    }

    std::vector<RecipeCommand> _commands;
    std::vector<RecipeCommand> _hopAdditions;  // Collected hop additions for BOIL
    int _currentStep = 0;
    float _currentTemp = 0;
    uint32_t _timerStart = 0;
    uint32_t _timerDuration = 0;
    uint32_t _pauseStartMs = 0;       // P3 — set on pause, used in resume
    float _waitTempTolerance = 0.5f;
    RecipeState _pausedState = RecipeState::Idle;

    // Waiting flags for external events. These mirror to gState.waitingFor*
    // (set/cleared via setWaitFlag()) so RecoveryManager can serialize them.
    bool _waitingForBoil = false;
    bool _waitingForTimer = false;
    bool _waitingForRamp = false;

    // Command parsing lives in core/RecipeParser.h (shared with the
    // native conformance test so it exercises the real parser). The
    // STEP label → BrewingStep mapping stays here because it needs the
    // BrewingStep enum (which the parser header deliberately doesn't pull).
    static BrewingStep mapBrewingStep(const char* m) {
        using RecipeParser::equalsI;
        if (equalsI(m, "PRE_HEATING") || equalsI(m, "PREHEATING") || equalsI(m, "PRE-AQUECIMENTO")) return BrewingStep::PreHeating;
        if (equalsI(m, "MASHING")  || equalsI(m, "MOSTURA"))      return BrewingStep::Mashing;
        if (equalsI(m, "MASH_OUT") || equalsI(m, "MASHOUT") || equalsI(m, "MASH-OUT")) return BrewingStep::MashOut;
        if (equalsI(m, "SPARGE")   || equalsI(m, "LAVAGEM"))      return BrewingStep::Sparge;
        if (equalsI(m, "BOILING")  || equalsI(m, "BOIL") || equalsI(m, "FERVURA")) return BrewingStep::Boiling;
        if (equalsI(m, "HOPPING")  || equalsI(m, "LUPULAGEM"))    return BrewingStep::Hopping;
        if (equalsI(m, "COOLING")  || equalsI(m, "RESFRIAMENTO")) return BrewingStep::Cooling;
        if (equalsI(m, "DONE")     || equalsI(m, "CONCLUIDO"))    return BrewingStep::Done;
        return BrewingStep::None;
    }

    // ── Step Execution ──────────────────────────────────────

    void executeCurrentStep() {
        if (_currentStep >= (int)_commands.size()) {
            completeRecipe();
            return;
        }

        const RecipeCommand& cmd = _commands[_currentStep];

        switch (cmd.type) {
            // ── Temperature ─────────────────────────────────
            case RecipeCommandType::SetTemp:
                gState.targetTemp = cmd.value;
                bus().publish(EventType::SetpointChanged, cmd.value);
                bus().publish(EventType::HeaterStateChanged, true);
                DEBUG_PRINTF("[Recipe] SET_TEMP %.1f\n", cmd.value);
                advanceStep();
                break;

            case RecipeCommandType::WaitTemp:
                gState.recipeState = RecipeState::WaitingForTemperature;
                gState.waitingForTemp = true;  // P5 — for recovery
                _waitTempTolerance = cmd.value;
                DEBUG_PRINTF("[Recipe] WAIT_TEMP (tol=%.1f)\n", cmd.value);
                break;

            case RecipeCommandType::MashOut:
                // Set mash-out temperature and wait for it
                gState.targetTemp = cmd.value;
                gState.mashOutTemp = cmd.value;
                gState.mashOutEnabled = true;
                bus().publish(EventType::SetpointChanged, cmd.value);
                bus().publish(EventType::HeaterStateChanged, true);
                gState.recipeState = RecipeState::WaitingForTemperature;
                gState.waitingForTemp = true;  // P5
                _waitTempTolerance = 0.5f;
                DEBUG_PRINTF("[Recipe] MASH_OUT %.1f\n", cmd.value);
                break;

            // ── Timers ──────────────────────────────────────
            case RecipeCommandType::WaitTimer:
                // Internal recipe timer (does not use TimerPlugin)
                _timerStart = millis();
                _timerDuration = (uint32_t)(cmd.value * 60000);
                gState.recipeState = RecipeState::WaitingForTimer;
                gState.timerRemainingMs = _timerDuration;
                gState.waitingForTimer = true;  // P5
                DEBUG_PRINTF("[Recipe] WAIT_TIMER %.0f min\n", cmd.value);
                break;

            case RecipeCommandType::Timer:
                // Use TimerPlugin (with notifications)
                _waitingForTimer = true;
                gState.waitingForTimer = true;  // P5
                gState.recipeState = RecipeState::Preparing;
                bus().publish(EventType::TimerStartRequest, (int)(cmd.value * 60));
                DEBUG_PRINTF("[Recipe] TIMER %.0f min (via TimerPlugin)\n", cmd.value);
                break;

            case RecipeCommandType::Alarm:
                // Absolute alarm - requires RTC
                if (gState.rtcAvailable) {
                    _waitingForTimer = true;
                    gState.waitingForTimer = true;  // P5
                    gState.recipeState = RecipeState::Preparing;
                    char buf[8];
                    snprintf(buf, sizeof(buf), "%02d:%02d", (int)cmd.value, (int)cmd.value2);
                    bus().publish(EventType::TimerStartRequest, String(buf));
                    DEBUG_PRINTF("[Recipe] ALARM %s\n", buf);
                } else {
                    DEBUG_PRINTLN("[Recipe] ALARM skipped - no RTC");
                    advanceStep();
                }
                break;

            // ── Boil ────────────────────────────────────────
            case RecipeCommandType::Boil:
                // Start boil timer with collected hop additions
                _waitingForBoil = true;
                gState.waitingForBoil = true;  // P5
                gState.recipeState = RecipeState::Preparing;
                startBoilWithAdditions((uint16_t)cmd.value);
                DEBUG_PRINTF("[Recipe] BOIL %.0f min with %d additions\n",
                            cmd.value, _hopAdditions.size());
                break;

            case RecipeCommandType::AddHop:
                // Collect hop addition for next BOIL command
                _hopAdditions.push_back(cmd);
                DEBUG_PRINTF("[Recipe] ADD_HOP @%.0f min: %s\n", 
                            cmd.value, cmd.message.c_str());
                advanceStep();
                break;

            case RecipeCommandType::WaitBoil:
                // Wait for boil timer to complete
                if (gState.boilActive) {
                    _waitingForBoil = true;
                    gState.waitingForBoil = true;  // P5
                    gState.recipeState = RecipeState::Preparing;
                    DEBUG_PRINTLN("[Recipe] WAIT_BOIL");
                } else {
                    // Boil not active, skip
                    advanceStep();
                }
                break;

            // ── Ramp ────────────────────────────────────────
            case RecipeCommandType::Ramp:
                bus().publish(EventType::RampConfigChanged, cmd.value);
                DEBUG_PRINTF("[Recipe] RAMP %.1f °C/min\n", cmd.value);
                advanceStep();
                break;

            case RecipeCommandType::RampOff:
                bus().publish(EventType::RampConfigChanged, 0.0f);
                DEBUG_PRINTLN("[Recipe] RAMP_OFF");
                advanceStep();
                break;

            // ── Actuators ───────────────────────────────────
            case RecipeCommandType::PumpOn:
                bus().publish(EventType::PumpStateChanged, true);
                DEBUG_PRINTLN("[Recipe] PUMP_ON");
                advanceStep();
                break;

            case RecipeCommandType::PumpOff:
                bus().publish(EventType::PumpStateChanged, false);
                DEBUG_PRINTLN("[Recipe] PUMP_OFF");
                advanceStep();
                break;

            case RecipeCommandType::HeaterOn:
                bus().publish(EventType::HeaterStateChanged, true);
                DEBUG_PRINTLN("[Recipe] HEATER_ON");
                advanceStep();
                break;

            case RecipeCommandType::HeaterOff:
                bus().publish(EventType::HeaterStateChanged, false);
                DEBUG_PRINTLN("[Recipe] HEATER_OFF");
                advanceStep();
                break;

            // ── Confirmation ────────────────────────────────
            case RecipeCommandType::WaitConfirm:
                gState.recipeState = RecipeState::WaitingForConfirm;
                gState.waitingForConfirm = true;  // P5
                setStr(gState.confirmMessage, cmd.message);
                bus().publish(EventType::RecipeWaitConfirm, cmd.message);
                sendRecipeWaitBle(cmd.message.c_str());
                DEBUG_PRINTF("[Recipe] WAIT_CONFIRM: %s\n", cmd.message.c_str());
                break;

            // ── UI Step ─────────────────────────────────────
            case RecipeCommandType::Step: {
                // The parser only records the label; map it to a known
                // BrewingStep here (where the enum lives). Unrecognized labels
                // fall through to a custom step driven by the message.
                BrewingStep bs = mapBrewingStep(cmd.message.c_str());
                if (bs != BrewingStep::None) {
                    gState.brewingStep = bs;
                    gState.brewingStepCustom[0] = 0;
                } else {
                    gState.brewingStep = BrewingStep::None;
                    setStr(gState.brewingStepCustom, cmd.message);
                }
                DEBUG_PRINTF("[Recipe] STEP: %s\n", cmd.message.c_str());
                advanceStep();
                break;
            }

            default:
                advanceStep();
                break;
        }
    }

    void startBoilWithAdditions(uint16_t minutes) {
        // Publish boil start event with additions
        // BoilTimerPlugin listens to BoilStarted event
        
        // First, clear and set up additions via events
        // We'll use a JSON string to pass additions
        String additionsJson = "[";
        for (size_t i = 0; i < _hopAdditions.size(); i++) {
            if (i > 0) additionsJson += ",";
            additionsJson += "{\"min\":" + String((int)_hopAdditions[i].value);
            additionsJson += ",\"name\":\"" + _hopAdditions[i].message + "\"}";
        }
        additionsJson += "]";
        
        // Publish with minutes and additions JSON
        bus().publish(EventType::BoilStarted, (float)minutes, additionsJson);
        
        // Clear collected additions
        _hopAdditions.clear();
    }

    void checkTemperatureReached() {
        float diff = abs(_currentTemp - gState.targetTemp);
        if (diff <= _waitTempTolerance) {
            DEBUG_PRINTF("[Recipe] Temperature reached: %.1f (target %.1f)\n",
                         _currentTemp, gState.targetTemp);
            advanceStep();
        }
    }

    void checkTimerExpired() {
        uint32_t elapsed = millis() - _timerStart;
        if (elapsed >= _timerDuration) {
            gState.timerRemainingMs = 0;
            DEBUG_PRINTLN("[Recipe] Timer expired");
            advanceStep();
        } else {
            gState.timerRemainingMs = _timerDuration - elapsed;
        }
    }

    void advanceStep() {
        _currentStep++;
        gState.recipeStep = _currentStep;
        gState.recipeState = RecipeState::Running;
        // P5 — leaving any wait state means the corresponding flag is cleared
        // so a recovery snapshot taken between steps doesn't resume a wait
        // that already finished.
        clearWaitFlags();
        bus().publish(EventType::RecipeStepChanged, _currentStep);
        sendRecipeStepBle();
    }

    void clearWaitFlags() {
        gState.waitingForTemp    = false;
        gState.waitingForTimer   = false;
        gState.waitingForBoil    = false;
        gState.waitingForRamp    = false;
        gState.waitingForConfirm = false;
    }

    void completeRecipe() {
        gState.recipeState = RecipeState::Completed;
        gState.mode        = OperatingMode::Idle;
        clearWaitFlags();
        gState.recipePausedDurationMs = 0;
        bus().publish(EventType::RecipeCompleted);
        sendRecipeStateBle("completed");
        bus().publish(EventType::HeaterStateChanged, false);
        RecoveryManager::instance().clearRecovery();
        DEBUG_PRINTLN("[Recipe] Completed!");
    }
};
