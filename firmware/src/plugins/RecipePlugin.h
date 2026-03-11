#pragma once

#include <Arduino.h>
#include <vector>
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
                advanceStep();
            }
        });

        // Listen for timer completion (TimerPlugin)
        bus().subscribe(EventType::TimerCompleted, [this](const Event&) {
            if (_waitingForTimer) {
                _waitingForTimer = false;
                advanceStep();
            }
        });

        // Listen for ramp completion
        bus().subscribe(EventType::RampCompleted, [this](const Event&) {
            if (_waitingForRamp) {
                _waitingForRamp = false;
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

        // Parse line by line
        int start = 0;
        while (start < (int)content.length()) {
            int end = content.indexOf('\n', start);
            if (end < 0) end = content.length();

            String line = content.substring(start, end);
            line.trim();
            start = end + 1;

            if (line.length() == 0) continue;

            RecipeCommand cmd = parseLine(line);
            if (cmd.type != RecipeCommandType::Comment && 
                cmd.type != RecipeCommandType::Unknown) {
                _commands.push_back(cmd);
            }
        }

        gState.recipeName = name;
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
        _hopAdditions.clear();
        
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
        
        bus().publish(EventType::RecipeStopped);
        bus().publish(EventType::SetpointChanged, 0.0f);
        bus().publish(EventType::HeaterStateChanged, false);
        RecoveryManager::instance().clearRecovery();
        DEBUG_PRINTLN("[Recipe] Stopped");
    }

    void pause() {
        if (gState.recipeState != RecipeState::Idle &&
            gState.recipeState != RecipeState::Completed) {
            _pausedState = gState.recipeState;
            gState.recipeState = RecipeState::Paused;
            bus().publish(EventType::RecipePaused);
            DEBUG_PRINTLN("[Recipe] Paused");
        }
    }

    void resume() {
        if (gState.recipeState == RecipeState::Paused) {
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

    // Restore from recovery data
    bool restoreFromRecovery(const RecoveryData& data, const String& recipeContent) {
        if (!loadRecipe(recipeContent, String(data.recipeName))) {
            return false;
        }
        
        _currentStep = data.currentStep;
        gState.recipeStep = data.currentStep;
        gState.targetTemp = data.targetTemp;
        gState.recipeState = static_cast<RecipeState>(data.recipeState);
        gState.timerRemainingMs = data.timerRemainingMs;
        gState.mode = OperatingMode::Recipe;
        
        if (gState.recipeState == RecipeState::WaitingForTimer && data.timerRemainingMs > 0) {
            _timerDuration = data.timerRemainingMs;
            _timerStart = millis();
        }
        
        DEBUG_PRINTF("[Recipe] Restored: step %d, state %d, target %.1f\n",
                     _currentStep, data.recipeState, data.targetTemp);
        
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
    float _waitTempTolerance = 0.5f;
    RecipeState _pausedState = RecipeState::Idle;
    
    // Waiting flags for external events
    bool _waitingForBoil = false;
    bool _waitingForTimer = false;
    bool _waitingForRamp = false;

    // ── Command Parsing ─────────────────────────────────────

    RecipeCommand parseLine(const String& line) {
        RecipeCommand cmd;

        if (line.startsWith("#")) {
            cmd.type = RecipeCommandType::Comment;
            return cmd;
        }

        String upper = line;
        upper.toUpperCase();

        // Temperature commands
        if (upper.startsWith("SET_TEMP")) {
            cmd.type = RecipeCommandType::SetTemp;
            cmd.value = extractFloat(line, 8);
        }
        else if (upper.startsWith("WAIT_TEMP")) {
            cmd.type = RecipeCommandType::WaitTemp;
            float tol = extractFloat(line, 9);
            cmd.value = (tol > 0) ? tol : _waitTempTolerance;
        }
        else if (upper.startsWith("MASH_OUT")) {
            cmd.type = RecipeCommandType::MashOut;
            float temp = extractFloat(line, 8);
            cmd.value = (temp > 0) ? temp : 76.0f;  // Default 76°C
        }
        // Timer commands
        else if (upper.startsWith("WAIT_TIMER")) {
            cmd.type = RecipeCommandType::WaitTimer;
            cmd.value = extractFloat(line, 10);  // minutes
        }
        else if (upper.startsWith("TIMER")) {
            cmd.type = RecipeCommandType::Timer;
            cmd.value = extractFloat(line, 5);  // minutes
        }
        else if (upper.startsWith("ALARM")) {
            cmd.type = RecipeCommandType::Alarm;
            // Parse HH:MM format
            String timeStr = line.substring(5);
            timeStr.trim();
            int colonPos = timeStr.indexOf(':');
            if (colonPos > 0) {
                cmd.value = timeStr.substring(0, colonPos).toInt();   // hour
                cmd.value2 = timeStr.substring(colonPos + 1).toInt(); // minute
            }
        }
        // Boil commands
        else if (upper.startsWith("BOIL")) {
            cmd.type = RecipeCommandType::Boil;
            cmd.value = extractFloat(line, 4);  // minutes
        }
        else if (upper.startsWith("ADD_HOP")) {
            cmd.type = RecipeCommandType::AddHop;
            // Parse: ADD_HOP <minutes> "name"
            String rest = line.substring(7);
            rest.trim();
            int spacePos = rest.indexOf(' ');
            if (spacePos > 0) {
                cmd.value = rest.substring(0, spacePos).toFloat();  // minutes
                cmd.message = extractQuotedString(rest, spacePos);
            } else {
                cmd.value = rest.toFloat();
                cmd.message = "Hop";
            }
        }
        else if (upper.startsWith("WAIT_BOIL")) {
            cmd.type = RecipeCommandType::WaitBoil;
        }
        // Ramp commands
        else if (upper.startsWith("RAMP_OFF")) {
            cmd.type = RecipeCommandType::RampOff;
        }
        else if (upper.startsWith("RAMP")) {
            cmd.type = RecipeCommandType::Ramp;
            cmd.value = extractFloat(line, 4);  // °C/min
        }
        // Actuator commands
        else if (upper.startsWith("PUMP_ON")) {
            cmd.type = RecipeCommandType::PumpOn;
        }
        else if (upper.startsWith("PUMP_OFF")) {
            cmd.type = RecipeCommandType::PumpOff;
        }
        else if (upper.startsWith("HEATER_ON")) {
            cmd.type = RecipeCommandType::HeaterOn;
        }
        else if (upper.startsWith("HEATER_OFF")) {
            cmd.type = RecipeCommandType::HeaterOff;
        }
        // Confirmation
        else if (upper.startsWith("WAIT_CONFIRM")) {
            cmd.type = RecipeCommandType::WaitConfirm;
            cmd.message = extractQuotedString(line, 12);
            if (cmd.message.isEmpty()) {
                cmd.message = "Confirmar para continuar";
            }
        }
        // UI Step
        else if (upper.startsWith("STEP")) {
            cmd.type = RecipeCommandType::Step;
            cmd.message = extractQuotedString(line, 4);
            // Try to match known step names
            String stepUpper = cmd.message;
            stepUpper.toUpperCase();
            if (stepUpper == "PRE_HEATING" || stepUpper == "PREHEATING" || stepUpper == "PRE-AQUECIMENTO") {
                cmd.value = (float)BrewingStep::PreHeating;
            } else if (stepUpper == "MASHING" || stepUpper == "MOSTURA") {
                cmd.value = (float)BrewingStep::Mashing;
            } else if (stepUpper == "MASH_OUT" || stepUpper == "MASHOUT" || stepUpper == "MASH-OUT") {
                cmd.value = (float)BrewingStep::MashOut;
            } else if (stepUpper == "SPARGE" || stepUpper == "LAVAGEM") {
                cmd.value = (float)BrewingStep::Sparge;
            } else if (stepUpper == "BOILING" || stepUpper == "BOIL" || stepUpper == "FERVURA") {
                cmd.value = (float)BrewingStep::Boiling;
            } else if (stepUpper == "HOPPING" || stepUpper == "LUPULAGEM") {
                cmd.value = (float)BrewingStep::Hopping;
            } else if (stepUpper == "COOLING" || stepUpper == "RESFRIAMENTO") {
                cmd.value = (float)BrewingStep::Cooling;
            } else if (stepUpper == "DONE" || stepUpper == "CONCLUIDO") {
                cmd.value = (float)BrewingStep::Done;
            } else {
                cmd.value = 0;  // Custom step - use message
            }
        }
        else {
            cmd.type = RecipeCommandType::Unknown;
            DEBUG_PRINTF("[Recipe] Unknown command: %s\n", line.c_str());
        }

        return cmd;
    }

    float extractFloat(const String& line, int offset) {
        String rest = line.substring(offset);
        rest.trim();
        return rest.toFloat();
    }

    String extractQuotedString(const String& line, int offset) {
        String rest = line.substring(offset);
        rest.trim();
        int q1 = rest.indexOf('"');
        if (q1 < 0) return rest;
        int q2 = rest.indexOf('"', q1 + 1);
        if (q2 < 0) return rest.substring(q1 + 1);
        return rest.substring(q1 + 1, q2);
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
                _waitTempTolerance = cmd.value;
                DEBUG_PRINTF("[Recipe] WAIT_TEMP (tol=%.1f)\n", cmd.value);
                break;

            case RecipeCommandType::MashOut:
                // Set mash-out temperature and wait for it
                gState.targetTemp = cmd.value;
                gState.mashOutTemp = cmd.value;
                bus().publish(EventType::SetpointChanged, cmd.value);
                bus().publish(EventType::HeaterStateChanged, true);
                gState.recipeState = RecipeState::WaitingForTemperature;
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
                DEBUG_PRINTF("[Recipe] WAIT_TIMER %.0f min\n", cmd.value);
                break;

            case RecipeCommandType::Timer:
                // Use TimerPlugin (with notifications)
                _waitingForTimer = true;
                gState.recipeState = RecipeState::Preparing;
                // Publish event to start timer - TimerPlugin will handle it
                bus().publish(EventType::TimerStartRequest, (int)(cmd.value * 60));
                DEBUG_PRINTF("[Recipe] TIMER %.0f min (via TimerPlugin)\n", cmd.value);
                break;

            case RecipeCommandType::Alarm:
                // Absolute alarm - requires RTC
                if (gState.rtcAvailable) {
                    _waitingForTimer = true;
                    gState.recipeState = RecipeState::Preparing;
                    // Publish with HH:MM string for absolute timer
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
                gState.confirmMessage = cmd.message;
                bus().publish(EventType::RecipeWaitConfirm, cmd.message);
                DEBUG_PRINTF("[Recipe] WAIT_CONFIRM: %s\n", cmd.message.c_str());
                break;

            // ── UI Step ─────────────────────────────────────
            case RecipeCommandType::Step:
                if (cmd.value > 0) {
                    gState.brewingStep = (BrewingStep)(int)cmd.value;
                    gState.brewingStepCustom = "";
                } else {
                    gState.brewingStep = BrewingStep::None;
                    gState.brewingStepCustom = cmd.message;
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
        bus().publish(EventType::RecipeStepChanged, _currentStep);
    }

    void completeRecipe() {
        gState.recipeState = RecipeState::Completed;
        gState.mode = OperatingMode::Idle;
        bus().publish(EventType::RecipeCompleted);
        bus().publish(EventType::HeaterStateChanged, false);
        RecoveryManager::instance().clearRecovery();
        DEBUG_PRINTLN("[Recipe] Completed!");
    }
};
