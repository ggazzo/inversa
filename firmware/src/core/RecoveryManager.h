#pragma once

#include <Arduino.h>
#include "../models/MachineState.h"
#include "../plugins/SDCardPlugin.h"

// ─── Recovery Data Structure ────────────────────────────────
// Stored to SD card periodically during recipe execution
// to enable recovery after power loss.

#pragma pack(push, 1)
struct RecoveryData {
    uint32_t magic = 0x494E5652;  // "INVR" magic number
    uint8_t version = 1;
    
    // Recipe state
    char recipeName[64] = {0};
    uint8_t currentStep = 0;
    uint8_t totalSteps = 0;
    uint8_t recipeState = 0;  // RecipeState enum as uint8_t
    
    // Temperature
    float targetTemp = 0;
    
    // Timer state (if in WaitingForTimer)
    uint32_t timerRemainingMs = 0;
    
    // Timestamp for validation
    uint32_t savedAtMs = 0;
    
    // Checksum (simple XOR)
    uint8_t checksum = 0;
};
#pragma pack(pop)

// ─── Recovery Manager ───────────────────────────────────────
class RecoveryManager {
public:
    static RecoveryManager& instance() {
        static RecoveryManager inst;
        return inst;
    }

    void init(SDCardPlugin* sd) {
        _sd = sd;
    }

    // Check if recovery data exists and is valid
    bool hasValidRecovery() {
        if (!_sd || !_sd->isMounted()) return false;
        if (!_sd->hasRecoveryData()) return false;
        
        RecoveryData data;
        size_t read = _sd->loadRecoveryData((uint8_t*)&data, sizeof(data));
        if (read != sizeof(data)) return false;
        
        // Validate magic
        if (data.magic != 0x494E5652) return false;
        
        // Validate version
        if (data.version != 1) return false;
        
        // Validate checksum
        if (!validateChecksum(data)) return false;
        
        // Validate recipe name is not empty
        if (data.recipeName[0] == 0) return false;
        
        // Validate recipe state is valid for recovery
        auto state = static_cast<RecipeState>(data.recipeState);
        if (state == RecipeState::Idle || state == RecipeState::Completed) {
            return false;
        }
        
        Serial.printf("[Recovery] Valid recovery found: '%s' step %d/%d\n",
                     data.recipeName, data.currentStep, data.totalSteps);
        return true;
    }

    // Load recovery data
    bool loadRecovery(RecoveryData& data) {
        if (!_sd || !_sd->isMounted()) return false;
        
        size_t read = _sd->loadRecoveryData((uint8_t*)&data, sizeof(data));
        if (read != sizeof(data)) return false;
        
        if (data.magic != 0x494E5652) return false;
        if (!validateChecksum(data)) return false;
        
        return true;
    }

    // Save current recipe state
    bool saveRecovery() {
        if (!_sd || !_sd->isMounted()) return false;
        
        // Only save if recipe is actually running
        if (gState.mode != OperatingMode::Recipe) return false;
        if (gState.recipeState == RecipeState::Idle || 
            gState.recipeState == RecipeState::Completed) {
            return false;
        }
        
        RecoveryData data;
        data.magic = 0x494E5652;
        data.version = 1;
        
        // Copy recipe name (safely)
        strncpy(data.recipeName, gState.recipeName.c_str(), sizeof(data.recipeName) - 1);
        data.recipeName[sizeof(data.recipeName) - 1] = 0;
        
        data.currentStep = gState.recipeStep;
        data.totalSteps = gState.recipeTotalSteps;
        data.recipeState = static_cast<uint8_t>(gState.recipeState);
        data.targetTemp = gState.targetTemp;
        data.timerRemainingMs = gState.timerRemainingMs;
        data.savedAtMs = millis();
        
        // Calculate checksum
        data.checksum = calculateChecksum(data);
        
        bool ok = _sd->saveRecoveryData((uint8_t*)&data, sizeof(data));
        if (ok) {
            _lastSaveMs = millis();
            Serial.printf("[Recovery] Saved state: step %d, state %d\n", 
                         data.currentStep, data.recipeState);
        }
        return ok;
    }

    // Clear recovery data (call when recipe completes or is stopped)
    void clearRecovery() {
        if (_sd && _sd->isMounted()) {
            _sd->deleteRecoveryData();
            Serial.println("[Recovery] Cleared");
        }
    }

    // Periodic save in loop (call from RecipePlugin)
    void periodicSave() {
        if (millis() - _lastSaveMs >= RECOVERY_SAVE_INTERVAL_MS) {
            saveRecovery();
        }
    }

    // Get last save time
    uint32_t getLastSaveMs() const { return _lastSaveMs; }

private:
    RecoveryManager() = default;
    SDCardPlugin* _sd = nullptr;
    uint32_t _lastSaveMs = 0;

    uint8_t calculateChecksum(const RecoveryData& data) {
        uint8_t sum = 0;
        const uint8_t* ptr = (const uint8_t*)&data;
        // XOR all bytes except the checksum itself
        for (size_t i = 0; i < sizeof(data) - 1; i++) {
            sum ^= ptr[i];
        }
        return sum;
    }

    bool validateChecksum(const RecoveryData& data) {
        return data.checksum == calculateChecksum(data);
    }
};
