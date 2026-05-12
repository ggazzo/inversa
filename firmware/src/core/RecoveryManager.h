#pragma once

#include <Arduino.h>
#include "../models/MachineState.h"
#include "../plugins/SDCardPlugin.h"

// ─── Recovery Data Structure v2 (P5/P10/P11) ────────────────
// Stored to SD card periodically during recipe execution so a power loss
// mid-brew can be resumed instead of restarting from the beginning.
//
// V2 changes (2026-05):
//   - P10: currentStep/totalSteps widened to uint16_t (uint8 was inadequate
//          for long recipes — Inversa DSL allows ~hundreds of steps).
//   - P11: bootAttempts counter — if a restore crashes repeatedly, RecoveryManager
//          stops offering it, breaking boot loops.
//   - P5:  added brewingStep + mash-out + pause + minimal BoilTimer state. The
//          _hopAdditions list is NOT stored here. RecipePlugin reconciles it on
//          restore by walking _commands[0.._currentStep] for past HOP commands —
//          since the recipe content is reloaded from SD, the hop list is derived
//          state and would only bloat the recovery image.
//   - Migration v1→v2: hasValidRecovery() rejects v1 (returns false instead of
//          crashing on a sizeof mismatch). User loses unfinished recipes from the
//          previous firmware — acceptable on a one-shot upgrade.

#pragma pack(push, 1)
struct RecoveryData {
    uint32_t magic   = 0x494E5652;  // "INVR"
    uint8_t  version = 2;           // P5/P10/P11

    // ── Recipe state ────────────────────────────────────
    char     recipeName[64]  = {0};   // SD filename, INCLUDING .txt extension (P4)
    uint16_t currentStep     = 0;     // P10 — was uint8
    uint16_t totalSteps      = 0;     // P10
    uint8_t  recipeState     = 0;     // RecipeState enum
    uint32_t recipePausedDurationMs = 0;  // P5 — accumulated paused time

    // ── Temperature / setpoint ──────────────────────────
    float    targetTemp      = 0;

    // ── Brewing UI step (BrewingStep enum) ──────────────
    uint8_t  brewingStep     = 0;     // P5 — for UI label after resume
    bool     mashOutEnabled  = false;
    float    mashOutTemp     = 76.0f;

    // ── Generic Timer (in seconds remaining) ────────────
    uint32_t timerRemainingMs = 0;

    // ── BoilTimer minimal state (P5) ────────────────────
    bool     boilActive       = false;
    bool     boilPaused       = false;
    uint32_t boilTotalSec     = 0;
    uint32_t boilRemainingSec = 0;

    // ── Waiting-for flags (P5) so RecipePlugin can resume the right wait ─
    bool     waitingForTemp   = false;
    bool     waitingForTimer  = false;
    bool     waitingForBoil   = false;
    bool     waitingForRamp   = false;
    bool     waitingForConfirm = false;

    // ── Boot-loop guard (P11) ───────────────────────────
    uint8_t  bootAttempts    = 0;     // incremented at boot before user accepts

    // ── Diagnostics ─────────────────────────────────────
    uint32_t savedAtMs       = 0;     // millis() at save (informational)

    uint8_t  checksum        = 0;     // XOR of all preceding bytes
};
#pragma pack(pop)

// Hard ceiling — if the saved struct grew across versions, hasValidRecovery
// must still safely reject older sizes. We rely on SD::read returning the
// stored size and only accept exact matches.
static constexpr uint32_t RECOVERY_MAGIC          = 0x494E5652;
static constexpr uint8_t  RECOVERY_VERSION        = 2;
static constexpr uint8_t  RECOVERY_MAX_BOOT_ATTEMPTS = 3;  // P11

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
        strncpy(data.recipeName, gState.recipeName.c_str(), sizeof(data.recipeName) - 1);
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

    uint8_t calculateChecksum(const RecoveryData& data) {
        uint8_t sum = 0;
        const uint8_t* ptr = (const uint8_t*)&data;
        for (size_t i = 0; i < sizeof(data) - 1; i++) sum ^= ptr[i];
        return sum;
    }

    bool validateChecksum(const RecoveryData& data) {
        return data.checksum == calculateChecksum(data);
    }
};
