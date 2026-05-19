#pragma once

#include <Arduino.h>
#include <vector>
#include <cstring>
#include <cstdlib>
#include "../core/Plugin.h"
#include "../core/constants.h"
#include "../core/RecoveryManager.h"
#include "../models/MachineState.h"

// ─── Recipe Command Types ───────────────────────────────────
enum class RecipeCommandType : uint8_t {
    // Temperature
    SetTemp,        // SET_TEMP <value>
    WaitTemp,       // WAIT_TEMP [tolerance]
    MashOut,        // MASH_OUT [temp]  - go to mash-out temp and wait
    
    // Timers
    WaitTimer,      // WAIT_TIMER <minutes> - internal recipe timer
    Timer,          // TIMER <minutes> - use TimerPlugin (with notifications)
    Alarm,          // ALARM <HH:MM> - absolute timer (requires RTC)
    
    // Boil
    Boil,           // BOIL <minutes> - start boil timer
    AddHop,         // ADD_HOP <minutes> "name" - add hop addition to boil
    
    // Ramp mode
    Ramp,           // RAMP <rate> - enable gradual heating (°C/min)
    RampOff,        // RAMP_OFF - disable ramp mode
    
    // Actuators
    PumpOn,         // PUMP_ON
    PumpOff,        // PUMP_OFF
    HeaterOn,       // HEATER_ON
    HeaterOff,      // HEATER_OFF
    
    // Flow control
    WaitConfirm,    // WAIT_CONFIRM ["message"]
    WaitBoil,       // WAIT_BOIL - wait for boil timer to complete
    
    // UI/Visualization
    Step,           // STEP <name> - set brewing step for UI display
    
    // Other
    Comment,        // # comment line (ignored)
    Unknown
};

struct RecipeCommand {
    RecipeCommandType type = RecipeCommandType::Unknown;
    float value = 0;
    float value2 = 0;      // Second value (e.g., minute for ADD_HOP)
    String message = "";
};

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
                RecipeCommand cmd = parseLine(line);
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

    // ── Command Parsing ─────────────────────────────────────
    //
    // parseLine works on a NUL-terminated `const char*` and avoids any
    // intermediate `String` (no substring, no toUpperCase). The previous
    // implementation allocated 2–3 heap Strings per line; on a 50-step
    // recipe that meant ~150 transient allocations in a single load,
    // peaking heap usage right when SD I/O was also active.

    // Compare keyword `kw` (uppercase ASCII) to the beginning of `s`,
    // case-insensitively. Returns true if matched. Order of callers must
    // still put longer prefixes first (RAMP_OFF before RAMP, etc.).
    static bool startsWithI(const char* s, const char* kw) {
        while (*kw) {
            char a = *s++;
            char b = *kw++;
            if (a >= 'a' && a <= 'z') a = a - 32;
            if (a != b) return false;
        }
        return true;
    }

    static bool equalsI(const char* a, const char* b) {
        while (*a && *b) {
            char ca = *a++, cb = *b++;
            if (ca >= 'a' && ca <= 'z') ca -= 32;
            if (cb >= 'a' && cb <= 'z') cb -= 32;
            if (ca != cb) return false;
        }
        return *a == 0 && *b == 0;
    }

    // strtof_skip — advance past leading ws, parse a float, return value.
    static float parseFloatAt(const char* s) {
        while (*s == ' ' || *s == '\t') s++;
        return (float)strtod(s, nullptr);
    }

    // Extract the first quoted "..." segment from `s`. If no quote is
    // present, returns the (trimmed) rest of the line. Trailing whitespace
    // is stripped. Caller owns the resulting String.
    static String extractQuoted(const char* s) {
        while (*s == ' ' || *s == '\t') s++;
        const char* q1 = strchr(s, '"');
        if (q1) {
            const char* q2 = strchr(q1 + 1, '"');
            if (!q2) return String(q1 + 1);
            return String(q1 + 1).substring(0, (int)(q2 - q1 - 1));
        }
        // No quotes — return the (right-trimmed) rest.
        size_t n = strlen(s);
        while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t')) n--;
        String out;
        out.concat(s, n);
        return out;
    }

    RecipeCommand parseLine(const char* line) {
        RecipeCommand cmd;
        while (*line == ' ' || *line == '\t') line++;

        if (*line == '#' || *line == 0) {
            cmd.type = RecipeCommandType::Comment;
            return cmd;
        }

        if (startsWithI(line, "SET_TEMP")) {
            cmd.type = RecipeCommandType::SetTemp;
            cmd.value = parseFloatAt(line + 8);
        }
        else if (startsWithI(line, "WAIT_TEMP")) {
            cmd.type = RecipeCommandType::WaitTemp;
            float tol = parseFloatAt(line + 9);
            cmd.value = (tol > 0) ? tol : _waitTempTolerance;
        }
        else if (startsWithI(line, "MASH_OUT")) {
            cmd.type = RecipeCommandType::MashOut;
            float temp = parseFloatAt(line + 8);
            cmd.value = (temp > 0) ? temp : 76.0f;  // Default 76°C
        }
        else if (startsWithI(line, "WAIT_TIMER")) {
            cmd.type = RecipeCommandType::WaitTimer;
            cmd.value = parseFloatAt(line + 10);
        }
        else if (startsWithI(line, "TIMER")) {
            cmd.type = RecipeCommandType::Timer;
            cmd.value = parseFloatAt(line + 5);
        }
        else if (startsWithI(line, "ALARM")) {
            cmd.type = RecipeCommandType::Alarm;
            const char* p = line + 5;
            while (*p == ' ' || *p == '\t') p++;
            char* end = nullptr;
            long hour = strtol(p, &end, 10);
            if (end && *end == ':') {
                long minute = strtol(end + 1, nullptr, 10);
                cmd.value  = (float)hour;
                cmd.value2 = (float)minute;
            }
        }
        else if (startsWithI(line, "BOIL")) {
            cmd.type = RecipeCommandType::Boil;
            cmd.value = parseFloatAt(line + 4);
        }
        else if (startsWithI(line, "ADD_HOP")) {
            cmd.type = RecipeCommandType::AddHop;
            const char* rest = line + 7;
            while (*rest == ' ' || *rest == '\t') rest++;
            char* end = nullptr;
            cmd.value = strtof(rest, &end);
            if (end && end != rest) {
                cmd.message = extractQuoted(end);
                if (cmd.message.isEmpty()) cmd.message = "Hop";
            } else {
                cmd.message = "Hop";
            }
        }
        else if (startsWithI(line, "WAIT_BOIL")) {
            cmd.type = RecipeCommandType::WaitBoil;
        }
        else if (startsWithI(line, "RAMP_OFF")) {
            cmd.type = RecipeCommandType::RampOff;
        }
        else if (startsWithI(line, "RAMP")) {
            cmd.type = RecipeCommandType::Ramp;
            cmd.value = parseFloatAt(line + 4);
        }
        else if (startsWithI(line, "PUMP_ON")) {
            cmd.type = RecipeCommandType::PumpOn;
        }
        else if (startsWithI(line, "PUMP_OFF")) {
            cmd.type = RecipeCommandType::PumpOff;
        }
        else if (startsWithI(line, "HEATER_ON")) {
            cmd.type = RecipeCommandType::HeaterOn;
        }
        else if (startsWithI(line, "HEATER_OFF")) {
            cmd.type = RecipeCommandType::HeaterOff;
        }
        else if (startsWithI(line, "WAIT_CONFIRM")) {
            cmd.type = RecipeCommandType::WaitConfirm;
            cmd.message = extractQuoted(line + 12);
            if (cmd.message.isEmpty()) {
                cmd.message = "Confirmar para continuar";
            }
        }
        else if (startsWithI(line, "STEP")) {
            cmd.type = RecipeCommandType::Step;
            cmd.message = extractQuoted(line + 4);
            const char* m = cmd.message.c_str();
            if (equalsI(m, "PRE_HEATING") || equalsI(m, "PREHEATING") || equalsI(m, "PRE-AQUECIMENTO")) {
                cmd.value = (float)BrewingStep::PreHeating;
            } else if (equalsI(m, "MASHING") || equalsI(m, "MOSTURA")) {
                cmd.value = (float)BrewingStep::Mashing;
            } else if (equalsI(m, "MASH_OUT") || equalsI(m, "MASHOUT") || equalsI(m, "MASH-OUT")) {
                cmd.value = (float)BrewingStep::MashOut;
            } else if (equalsI(m, "SPARGE") || equalsI(m, "LAVAGEM")) {
                cmd.value = (float)BrewingStep::Sparge;
            } else if (equalsI(m, "BOILING") || equalsI(m, "BOIL") || equalsI(m, "FERVURA")) {
                cmd.value = (float)BrewingStep::Boiling;
            } else if (equalsI(m, "HOPPING") || equalsI(m, "LUPULAGEM")) {
                cmd.value = (float)BrewingStep::Hopping;
            } else if (equalsI(m, "COOLING") || equalsI(m, "RESFRIAMENTO")) {
                cmd.value = (float)BrewingStep::Cooling;
            } else if (equalsI(m, "DONE") || equalsI(m, "CONCLUIDO")) {
                cmd.value = (float)BrewingStep::Done;
            } else {
                cmd.value = 0;  // Custom step — UI uses message
            }
        }
        else {
            cmd.type = RecipeCommandType::Unknown;
            DEBUG_PRINTF("[Recipe] Unknown command: %s\n", line);
        }

        return cmd;
    }

    // Back-compat shim — the native test suite still calls parseLine with a
    // String. Kept inline so it folds into the char* path.
    RecipeCommand parseLine(const String& line) {
        return parseLine(line.c_str());
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
                DEBUG_PRINTF("[Recipe] WAIT_CONFIRM: %s\n", cmd.message.c_str());
                break;

            // ── UI Step ─────────────────────────────────────
            case RecipeCommandType::Step:
                if (cmd.value > 0) {
                    gState.brewingStep = (BrewingStep)(int)cmd.value;
                    gState.brewingStepCustom[0] = 0;
                } else {
                    gState.brewingStep = BrewingStep::None;
                    setStr(gState.brewingStepCustom, cmd.message);
                }
                DEBUG_PRINTF("[Recipe] STEP: %s\n", cmd.message.c_str());
                advanceStep();
                break;

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
        bus().publish(EventType::HeaterStateChanged, false);
        RecoveryManager::instance().clearRecovery();
        DEBUG_PRINTLN("[Recipe] Completed!");
    }
};
