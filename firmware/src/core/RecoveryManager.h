#pragma once

#include <Arduino.h>
#include "RecoveryData.h"          // packed struct + checksum (testable on native)
#include "../models/MachineState.h"
#include "../plugins/SDCardPlugin.h"

// ─── Recovery Manager (v2) — SD I/O + boot-loop guard ───────
// The RecoveryData struct, magic, version constants, and XOR checksum live in
// `core/RecoveryData.h` so they can be exercised by the native test runner
// without dragging in SD/Arduino. This class adds SD reads/writes,
// gState-snapshotting, and the bootAttempts increment.
//
// V2 binary format change is one-way: hasValidRecovery() rejects v1 and
// discards the file rather than crashing on a sizeof mismatch.

// ─── Recovery Manager ───────────────────────────────────────
class RecoveryManager {
public:
    static RecoveryManager& instance() {
        static RecoveryManager inst;
        return inst;
    }

    void init(SDCardPlugin* sd) { _sd = sd; }

    // P11 — call once at boot, BEFORE checking hasValidRecovery, so that if
    // the user accepts and the firmware crashes during restore, the next boot
    // sees an incremented count. After N failures we refuse and clear.
    void noteBootAttempt() {
        if (!_sd || !_sd->isMounted() || !_sd->hasRecoveryData()) return;
        RecoveryData data;
        size_t read = _sd->loadRecoveryData((uint8_t*)&data, sizeof(data));
        if (read != sizeof(data)) return;
        if (data.magic != RECOVERY_MAGIC || data.version != RECOVERY_VERSION) return;
        if (!validateChecksum(data)) return;

        if (data.bootAttempts >= RECOVERY_MAX_BOOT_ATTEMPTS) {
            DEBUG_PRINTF("[Recovery] bootAttempts=%u >= %u — clearing to break loop\n",
                         data.bootAttempts, RECOVERY_MAX_BOOT_ATTEMPTS);
            _sd->deleteRecoveryData();
            return;
        }

        data.bootAttempts++;
        data.checksum = calculateChecksum(data);
        _sd->saveRecoveryData((uint8_t*)&data, sizeof(data));
        DEBUG_PRINTF("[Recovery] bootAttempts=%u (limit %u)\n",
                     data.bootAttempts, RECOVERY_MAX_BOOT_ATTEMPTS);
    }

    // 8 layered validations (magic, version, size, checksum, name, state,
    // currentStep, bootAttempts).
    bool hasValidRecovery() {
        if (!_sd || !_sd->isMounted())      return false;
        if (!_sd->hasRecoveryData())        return false;

        RecoveryData data;
        size_t read = _sd->loadRecoveryData((uint8_t*)&data, sizeof(data));
        if (read != sizeof(data))           return false;          // (1) size
        if (data.magic   != RECOVERY_MAGIC) return false;          // (2) magic
        if (data.version != RECOVERY_VERSION) {                    // (3) version
            DEBUG_PRINTF("[Recovery] v%u found — incompatible with v%u, discarding\n",
                         data.version, RECOVERY_VERSION);
            _sd->deleteRecoveryData();
            return false;
        }
        if (!validateChecksum(data))        return false;          // (4) checksum
        if (data.recipeName[0] == 0)        return false;          // (5) name
        if (data.currentStep > data.totalSteps && data.totalSteps != 0) return false;  // (6) step bounds
        auto state = static_cast<RecipeState>(data.recipeState);
        if (state == RecipeState::Idle || state == RecipeState::Completed) {           // (7) state
            return false;
        }
        if (data.bootAttempts >= RECOVERY_MAX_BOOT_ATTEMPTS) {                         // (8) boot loop
            return false;
        }

        DEBUG_PRINTF("[Recovery] Valid v2 recovery: '%s' step %u/%u state=%u attempts=%u\n",
                     data.recipeName, data.currentStep, data.totalSteps,
                     data.recipeState, data.bootAttempts);
        return true;
    }

    bool loadRecovery(RecoveryData& data) {
        if (!_sd || !_sd->isMounted()) return false;
        size_t read = _sd->loadRecoveryData((uint8_t*)&data, sizeof(data));
        if (read != sizeof(data)) return false;
        if (data.magic != RECOVERY_MAGIC) return false;
        if (data.version != RECOVERY_VERSION) return false;
        if (!validateChecksum(data)) return false;
        return true;
    }

    // Snapshot current gState into the recovery file. Called periodically
    // (5s) from the Recipe runtime loop while mode==Recipe and recipe is
    // active (not Idle/Completed).
    bool saveRecovery() {
        if (!_sd || !_sd->isMounted()) return false;
        if (gState.mode != OperatingMode::Recipe) return false;
        if (gState.recipeState == RecipeState::Idle ||
            gState.recipeState == RecipeState::Completed) {
            return false;
        }

        RecoveryData data;
        data.magic   = RECOVERY_MAGIC;
        data.version = RECOVERY_VERSION;

        // Filename — INCLUDING ".txt" extension. P4 fix: stop concatenating
        // ".txt" at resume time; the suffix is authoritative here.
        strncpy(data.recipeName, gState.recipeName, sizeof(data.recipeName) - 1);
        data.recipeName[sizeof(data.recipeName) - 1] = 0;

        data.currentStep            = gState.recipeStep;
        data.totalSteps             = gState.recipeTotalSteps;
        data.recipeState            = static_cast<uint8_t>(gState.recipeState);
        data.recipePausedDurationMs = gState.recipePausedDurationMs;
        data.targetTemp             = gState.targetTemp;
        data.brewingStep            = static_cast<uint8_t>(gState.brewingStep);
        data.mashOutEnabled         = gState.mashOutEnabled;
        data.mashOutTemp            = gState.mashOutTemp;
        data.timerRemainingMs       = gState.timerRemainingMs;
        data.boilActive             = gState.boilActive;
        data.boilPaused             = gState.boilPaused;
        data.boilTotalSec           = gState.boilTotal;
        data.boilRemainingSec       = gState.boilRemaining;
        data.waitingForTemp         = gState.waitingForTemp;
        data.waitingForTimer        = gState.waitingForTimer;
        data.waitingForBoil         = gState.waitingForBoil;
        data.waitingForRamp         = gState.waitingForRamp;
        data.waitingForConfirm      = gState.waitingForConfirm;

        // Preserve bootAttempts across periodic saves so a successful restore
        // doesn't reset the counter back to 0 mid-recipe (a later crash would
        // get a fresh budget). Once the user confirms recovery succeeded, the
        // RecipePlugin clears the file via clearRecovery() — that resets the
        // counter to 0 implicitly.
        data.bootAttempts = _liveBootAttempts;

        data.savedAtMs = millis();
        data.checksum  = calculateChecksum(data);

        bool ok = _sd->saveRecoveryData((uint8_t*)&data, sizeof(data));
        if (ok) {
            _lastSaveMs = millis();
        }
        return ok;
    }

    // Capture the bootAttempts the recipe was loaded with, so subsequent
    // saveRecovery() calls preserve it. RecipePlugin::restoreFromRecovery
    // calls this with the recovered data.
    void setLiveBootAttempts(uint8_t n) { _liveBootAttempts = n; }

    void clearRecovery() {
        if (_sd && _sd->isMounted()) {
            _sd->deleteRecoveryData();
            _liveBootAttempts = 0;
            DEBUG_PRINTLN("[Recovery] Cleared");
        }
    }

    void periodicSave() {
        if (millis() - _lastSaveMs >= RECOVERY_SAVE_INTERVAL_MS) {
            saveRecovery();
        }
    }

    uint32_t getLastSaveMs() const { return _lastSaveMs; }

private:
    RecoveryManager() = default;
    SDCardPlugin* _sd = nullptr;
    uint32_t      _lastSaveMs = 0;
    uint8_t       _liveBootAttempts = 0;

    // Local thin wrappers around RecoveryData.h helpers. Kept so the rest of
    // this class reads naturally; both delegate to the testable functions.
    static uint8_t calculateChecksum(const RecoveryData& data) {
        return recoveryChecksum(data);
    }
    static bool validateChecksum(const RecoveryData& data) {
        return recoveryChecksumValid(data);
    }
};
