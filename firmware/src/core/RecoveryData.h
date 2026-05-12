#pragma once

#include <cstdint>
#include <cstddef>

// ─── Recovery Data v2 (P5/P10/P11) ──────────────────────────
// Packed struct serialized to `/recovery.bin` so a power-cut mid-brew can
// resume. Header is split out from RecoveryManager so the binary format,
// magic, version, and XOR checksum are testable without dragging in
// Arduino / SD / EventBus dependencies. RecoveryManager owns SD I/O.

constexpr uint32_t RECOVERY_MAGIC               = 0x494E5652;  // "INVR"
constexpr uint8_t  RECOVERY_VERSION             = 2;
constexpr uint8_t  RECOVERY_MAX_BOOT_ATTEMPTS   = 3;

#pragma pack(push, 1)
struct RecoveryData {
    uint32_t magic   = RECOVERY_MAGIC;
    uint8_t  version = RECOVERY_VERSION;

    // Recipe state
    char     recipeName[64]            = {0};   // includes ".txt" suffix (P4)
    uint16_t currentStep               = 0;     // P10 — was uint8
    uint16_t totalSteps                = 0;     // P10
    uint8_t  recipeState               = 0;     // RecipeState enum
    uint32_t recipePausedDurationMs    = 0;     // P5

    // Temperature / setpoint
    float    targetTemp                = 0;

    // Brewing UI step (BrewingStep enum)
    uint8_t  brewingStep               = 0;     // P5
    bool     mashOutEnabled            = false;
    float    mashOutTemp               = 76.0f;

    // Generic Timer (in ms remaining)
    uint32_t timerRemainingMs          = 0;

    // BoilTimer minimal state (P5)
    bool     boilActive                = false;
    bool     boilPaused                = false;
    uint32_t boilTotalSec              = 0;
    uint32_t boilRemainingSec          = 0;

    // Waiting-for flags (P5)
    bool     waitingForTemp            = false;
    bool     waitingForTimer           = false;
    bool     waitingForBoil            = false;
    bool     waitingForRamp            = false;
    bool     waitingForConfirm         = false;

    // Boot-loop guard (P11)
    uint8_t  bootAttempts              = 0;

    // Diagnostics
    uint32_t savedAtMs                 = 0;

    uint8_t  checksum                  = 0;     // XOR of all preceding bytes
};
#pragma pack(pop)

// XOR over every byte of the struct except the trailing `checksum` byte.
inline uint8_t recoveryChecksum(const RecoveryData& data) {
    uint8_t sum = 0;
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&data);
    for (size_t i = 0; i < sizeof(data) - 1; i++) sum ^= ptr[i];
    return sum;
}

inline bool recoveryChecksumValid(const RecoveryData& data) {
    return data.checksum == recoveryChecksum(data);
}
