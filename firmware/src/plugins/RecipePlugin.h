#pragma once

#include <Arduino.h>
#include <vector>
#include "../core/Plugin.h"
#include "../core/constants.h"
#include "../models/MachineState.h"

// ─── Recipe Command Types ───────────────────────────────────
enum class RecipeCommandType : uint8_t {
    SetTemp,        // SET_TEMP <value>
    WaitTemp,       // WAIT_TEMP [tolerance]
    WaitTimer,      // WAIT_TIMER <minutes>
    WaitConfirm,    // WAIT_CONFIRM ["message"]
    PumpOn,         // PUMP_ON
    PumpOff,        // PUMP_OFF
    Comment,        // # comment line (ignored)
    Unknown
};

struct RecipeCommand {
    RecipeCommandType type = RecipeCommandType::Unknown;
    float value = 0;
    String message = "";
};

// ─── Recipe Plugin ──────────────────────────────────────────
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

        Serial.println("[Recipe] Engine initialized");
        return true;
    }

    void loop() override {
        if (gState.recipeState == RecipeState::Idle ||
            gState.recipeState == RecipeState::Completed ||
            gState.recipeState == RecipeState::Paused) {
            return;
        }

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

            default:
                break;
        }
    }

    // ── Public API ──────────────────────────────────────────

    bool loadRecipe(const String& content, const String& name) {
        _commands.clear();
        _currentStep = 0;

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

        Serial.printf("[Recipe] Loaded '%s' with %d steps\n", 
                      name.c_str(), _commands.size());
        return !_commands.empty();
    }

    void start() {
        if (_commands.empty()) return;
        _currentStep = 0;
        gState.recipeState = RecipeState::Running;
        gState.recipeStep = 0;
        gState.mode = OperatingMode::Recipe;
        bus().publish(EventType::RecipeStarted, gState.recipeName);
        bus().publish(EventType::RecipeStepChanged, 0);
        Serial.println("[Recipe] Started");
    }

    void stop() {
        gState.recipeState = RecipeState::Idle;
        gState.mode = OperatingMode::Idle;
        gState.targetTemp = 0;
        gState.timerRemainingMs = 0;
        bus().publish(EventType::RecipeStopped);
        bus().publish(EventType::SetpointChanged, 0.0f);
        bus().publish(EventType::HeaterStateChanged, false);
        Serial.println("[Recipe] Stopped");
    }

    void pause() {
        if (gState.recipeState != RecipeState::Idle &&
            gState.recipeState != RecipeState::Completed) {
            _pausedState = gState.recipeState;
            gState.recipeState = RecipeState::Paused;
            bus().publish(EventType::RecipePaused);
            Serial.println("[Recipe] Paused");
        }
    }

    void resume() {
        if (gState.recipeState == RecipeState::Paused) {
            gState.recipeState = _pausedState;
            bus().publish(EventType::RecipeResumed);
            Serial.println("[Recipe] Resumed");
        }
    }

    void confirm() {
        bus().publish(EventType::RecipeConfirmed);
    }

    int getCurrentStep() const { return _currentStep; }
    int getTotalSteps() const { return _commands.size(); }
    const std::vector<RecipeCommand>& getCommands() const { return _commands; }

private:
    std::vector<RecipeCommand> _commands;
    int _currentStep = 0;
    float _currentTemp = 0;
    uint32_t _timerStart = 0;
    uint32_t _timerDuration = 0;
    float _waitTempTolerance = 0.5f;
    RecipeState _pausedState = RecipeState::Idle;

    // ── Command Parsing ─────────────────────────────────────

    RecipeCommand parseLine(const String& line) {
        RecipeCommand cmd;

        if (line.startsWith("#")) {
            cmd.type = RecipeCommandType::Comment;
            return cmd;
        }

        String upper = line;
        upper.toUpperCase();

        if (upper.startsWith("SET_TEMP")) {
            cmd.type = RecipeCommandType::SetTemp;
            cmd.value = extractFloat(line, 8);
        }
        else if (upper.startsWith("WAIT_TEMP")) {
            cmd.type = RecipeCommandType::WaitTemp;
            float tol = extractFloat(line, 9);
            cmd.value = (tol > 0) ? tol : _waitTempTolerance;
        }
        else if (upper.startsWith("WAIT_TIMER")) {
            cmd.type = RecipeCommandType::WaitTimer;
            cmd.value = extractFloat(line, 10);  // minutes
        }
        else if (upper.startsWith("WAIT_CONFIRM")) {
            cmd.type = RecipeCommandType::WaitConfirm;
            cmd.message = extractQuotedString(line, 12);
            if (cmd.message.isEmpty()) {
                cmd.message = "Confirm to continue";
            }
        }
        else if (upper.startsWith("PUMP_ON")) {
            cmd.type = RecipeCommandType::PumpOn;
        }
        else if (upper.startsWith("PUMP_OFF")) {
            cmd.type = RecipeCommandType::PumpOff;
        }
        else {
            cmd.type = RecipeCommandType::Unknown;
            Serial.printf("[Recipe] Unknown command: %s\n", line.c_str());
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
        if (q1 < 0) return rest;  // no quotes, return as-is
        int q2 = rest.indexOf('"', q1 + 1);
        if (q2 < 0) return rest.substring(q1 + 1);
        return rest.substring(q1 + 1, q2);
    }

    // ── Step Execution ──────────────────────────────────────

    void executeCurrentStep() {
        if (_currentStep >= (int)_commands.size()) {
            gState.recipeState = RecipeState::Completed;
            gState.mode = OperatingMode::Idle;
            bus().publish(EventType::RecipeCompleted);
            bus().publish(EventType::HeaterStateChanged, false);
            Serial.println("[Recipe] Completed!");
            return;
        }

        const RecipeCommand& cmd = _commands[_currentStep];

        switch (cmd.type) {
            case RecipeCommandType::SetTemp:
                gState.targetTemp = cmd.value;
                bus().publish(EventType::SetpointChanged, cmd.value);
                bus().publish(EventType::HeaterStateChanged, true);
                Serial.printf("[Recipe] SET_TEMP %.1f\n", cmd.value);
                advanceStep();
                break;

            case RecipeCommandType::WaitTemp:
                gState.recipeState = RecipeState::WaitingForTemperature;
                _waitTempTolerance = cmd.value;
                Serial.printf("[Recipe] WAIT_TEMP (tol=%.1f)\n", cmd.value);
                break;

            case RecipeCommandType::WaitTimer:
                _timerStart = millis();
                _timerDuration = (uint32_t)(cmd.value * 60000);  // min to ms
                gState.recipeState = RecipeState::WaitingForTimer;
                gState.timerRemainingMs = _timerDuration;
                Serial.printf("[Recipe] WAIT_TIMER %.0f min\n", cmd.value);
                break;

            case RecipeCommandType::WaitConfirm:
                gState.recipeState = RecipeState::WaitingForConfirm;
                gState.confirmMessage = cmd.message;
                bus().publish(EventType::RecipeWaitConfirm, cmd.message);
                Serial.printf("[Recipe] WAIT_CONFIRM: %s\n", cmd.message.c_str());
                break;

            case RecipeCommandType::PumpOn:
                bus().publish(EventType::PumpStateChanged, true);
                Serial.println("[Recipe] PUMP_ON");
                advanceStep();
                break;

            case RecipeCommandType::PumpOff:
                bus().publish(EventType::PumpStateChanged, false);
                Serial.println("[Recipe] PUMP_OFF");
                advanceStep();
                break;

            default:
                advanceStep();
                break;
        }
    }

    void checkTemperatureReached() {
        float diff = abs(_currentTemp - gState.targetTemp);
        if (diff <= _waitTempTolerance) {
            Serial.printf("[Recipe] Temperature reached: %.1f (target %.1f)\n",
                         _currentTemp, gState.targetTemp);
            advanceStep();
        }
    }

    void checkTimerExpired() {
        uint32_t elapsed = millis() - _timerStart;
        if (elapsed >= _timerDuration) {
            gState.timerRemainingMs = 0;
            Serial.println("[Recipe] Timer expired");
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
};
