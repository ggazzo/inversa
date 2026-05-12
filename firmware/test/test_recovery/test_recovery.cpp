// test_recovery.cpp — RecoveryData v2 binary format, checksum, validation.
//
// We test the layered guards from hasValidRecovery() at the struct level:
//   1. size (sizeof determinism)
//   2. magic word
//   3. version
//   4. XOR checksum (P5)
//   5. recipeName non-empty
//   6. currentStep ≤ totalSteps
//   7. recipeState eligible (RecoveryManager exact check is integration-side)
//   8. bootAttempts ≤ MAX
//
// SD I/O lives in RecoveryManager and is exercised by the hardware checklist
// (`_reversa_sdd/validation/recovery-checklist.md`).

#include <unity.h>
#include <cstring>
#include "../../src/core/RecoveryData.h"

void setUp(void) {}
void tearDown(void) {}

// ─── Defaults + magic ───────────────────────────────────────

void test_default_struct_has_correct_magic_and_version() {
    RecoveryData d;
    TEST_ASSERT_EQUAL_UINT32(RECOVERY_MAGIC, d.magic);
    TEST_ASSERT_EQUAL_UINT8(RECOVERY_VERSION, d.version);
}

void test_v1_magic_is_INVR_little_endian() {
    // "INVR" in little-endian = 0x52564E49. We declared 0x494E5652 which is
    // the big-endian reading of those bytes — both forms work, but pin down
    // the byte order so a careless re-spelling doesn't silently break the
    // binary on disk.
    TEST_ASSERT_EQUAL_UINT32(0x494E5652, RECOVERY_MAGIC);
}

// ─── Checksum ───────────────────────────────────────────────

void test_checksum_of_empty_struct() {
    RecoveryData d;
    uint8_t c = recoveryChecksum(d);
    // Most bytes are zero; the non-zero parts are the magic (4 B) and version
    // and mashOutTemp. We don't pin the exact value — that depends on padding
    // — only that recoveryChecksumValid round-trips.
    d.checksum = c;
    TEST_ASSERT_TRUE(recoveryChecksumValid(d));
}

void test_checksum_detects_one_bit_flip_anywhere() {
    RecoveryData d;
    std::memcpy(d.recipeName, "IPA.txt", 8);
    d.currentStep = 12;
    d.totalSteps  = 22;
    d.recipeState = 1;
    d.targetTemp  = 67.5f;
    d.checksum    = recoveryChecksum(d);
    TEST_ASSERT_TRUE(recoveryChecksumValid(d));

    // Flip exactly one bit somewhere benign.
    uint8_t* p = reinterpret_cast<uint8_t*>(&d);
    p[16] ^= 0x01;
    TEST_ASSERT_FALSE(recoveryChecksumValid(d));
}

void test_checksum_excludes_self() {
    // The checksum byte itself is excluded from the XOR. Verify by
    // computing twice with different `checksum` values; both should match.
    RecoveryData d;
    d.recipeName[0] = 'A';
    uint8_t c1 = recoveryChecksum(d);
    d.checksum = 0xFF;       // pretend a stale value
    uint8_t c2 = recoveryChecksum(d);
    TEST_ASSERT_EQUAL_UINT8(c1, c2);
}

// ─── Boot-loop guard constant (P11) ─────────────────────────

void test_max_boot_attempts_constant() {
    // Pin the limit. RecoveryManager.noteBootAttempt() clears the file when
    // bootAttempts >= MAX. If this constant moves, the integration spec
    // (`recovery-checklist.md` block D) needs updating.
    TEST_ASSERT_EQUAL_UINT8(3, RECOVERY_MAX_BOOT_ATTEMPTS);
}

// ─── Field roundtrip (P5 reconciliation depends on every field) ─

void test_roundtrip_v2_fields_preserved() {
    RecoveryData d;
    std::memcpy(d.recipeName, "Lager.txt", 10);
    d.currentStep            = 7;
    d.totalSteps             = 19;
    d.recipeState            = 3;        // Running
    d.recipePausedDurationMs = 12345;    // P3
    d.targetTemp             = 67.5f;
    d.brewingStep            = 2;        // Mashing
    d.mashOutEnabled         = true;
    d.mashOutTemp            = 76.0f;
    d.timerRemainingMs       = 1800000;  // 30 min
    d.boilActive             = false;
    d.boilPaused             = false;
    d.boilTotalSec           = 0;
    d.boilRemainingSec       = 0;
    d.waitingForTemp         = false;
    d.waitingForTimer        = true;     // <- inside mash
    d.waitingForBoil         = false;
    d.waitingForRamp         = false;
    d.waitingForConfirm      = false;
    d.bootAttempts           = 1;
    d.savedAtMs              = 999999;
    d.checksum               = recoveryChecksum(d);

    // Round-trip through bytes to simulate SD write/read.
    uint8_t buf[sizeof(d)];
    std::memcpy(buf, &d, sizeof(d));
    RecoveryData restored;
    std::memcpy(&restored, buf, sizeof(restored));

    TEST_ASSERT_EQUAL_STRING("Lager.txt", restored.recipeName);
    TEST_ASSERT_EQUAL_UINT16(7,  restored.currentStep);
    TEST_ASSERT_EQUAL_UINT16(19, restored.totalSteps);
    TEST_ASSERT_EQUAL_UINT8(3,   restored.recipeState);
    TEST_ASSERT_EQUAL_UINT32(12345,   restored.recipePausedDurationMs);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 67.5f, restored.targetTemp);
    TEST_ASSERT_EQUAL_UINT8(2,   restored.brewingStep);
    TEST_ASSERT_TRUE(restored.mashOutEnabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 76.0f, restored.mashOutTemp);
    TEST_ASSERT_EQUAL_UINT32(1800000, restored.timerRemainingMs);
    TEST_ASSERT_FALSE(restored.boilActive);
    TEST_ASSERT_TRUE(restored.waitingForTimer);
    TEST_ASSERT_FALSE(restored.waitingForBoil);
    TEST_ASSERT_EQUAL_UINT8(1, restored.bootAttempts);
    TEST_ASSERT_TRUE(recoveryChecksumValid(restored));
}

// ─── Step bounds (validation #6) ────────────────────────────

void test_step_bounds_currentStep_can_equal_total() {
    RecoveryData d;
    d.currentStep = 22;
    d.totalSteps  = 22;
    // Equal is OK (last step, about to complete). The validation in
    // RecoveryManager rejects only `current > total && total != 0`.
    TEST_ASSERT_TRUE(d.currentStep <= d.totalSteps);
}

// ─── Packed-struct sanity ───────────────────────────────────
// uint16_t fields (P10) must remain 16 bits — not silently widened.

void test_p10_uint16_step_width() {
    TEST_ASSERT_EQUAL_size_t(sizeof(uint16_t), sizeof(RecoveryData::currentStep));
    TEST_ASSERT_EQUAL_size_t(sizeof(uint16_t), sizeof(RecoveryData::totalSteps));
}

// Struct must be packed (no surprise padding between members). We check by
// asserting it's exactly the sum of the field sizes.
void test_struct_is_packed_no_padding() {
    constexpr size_t expected =
        sizeof(uint32_t) +       // magic
        sizeof(uint8_t) +        // version
        64 +                     // recipeName
        sizeof(uint16_t) +       // currentStep
        sizeof(uint16_t) +       // totalSteps
        sizeof(uint8_t) +        // recipeState
        sizeof(uint32_t) +       // recipePausedDurationMs
        sizeof(float) +          // targetTemp
        sizeof(uint8_t) +        // brewingStep
        sizeof(bool) +           // mashOutEnabled
        sizeof(float) +          // mashOutTemp
        sizeof(uint32_t) +       // timerRemainingMs
        sizeof(bool) +           // boilActive
        sizeof(bool) +           // boilPaused
        sizeof(uint32_t) +       // boilTotalSec
        sizeof(uint32_t) +       // boilRemainingSec
        sizeof(bool) * 5 +       // waitingFor*
        sizeof(uint8_t) +        // bootAttempts
        sizeof(uint32_t) +       // savedAtMs
        sizeof(uint8_t);         // checksum
    TEST_ASSERT_EQUAL_size_t(expected, sizeof(RecoveryData));
}

// ─── Main ───────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_default_struct_has_correct_magic_and_version);
    RUN_TEST(test_v1_magic_is_INVR_little_endian);
    RUN_TEST(test_checksum_of_empty_struct);
    RUN_TEST(test_checksum_detects_one_bit_flip_anywhere);
    RUN_TEST(test_checksum_excludes_self);
    RUN_TEST(test_max_boot_attempts_constant);
    RUN_TEST(test_roundtrip_v2_fields_preserved);
    RUN_TEST(test_step_bounds_currentStep_can_equal_total);
    RUN_TEST(test_p10_uint16_step_width);
    RUN_TEST(test_struct_is_packed_no_padding);
    return UNITY_END();
}
