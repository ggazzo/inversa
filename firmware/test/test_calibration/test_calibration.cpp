// test_calibration.cpp — unit tests for the temperature-calibration
// math (T_real = slope · T_medido + offset).
//
// The actual application of this transformation lives in
// `firmware/src/plugins/TemperaturePlugin.h::loop()`, where it runs
// after the Kalman filter and before `gState.currentTemp` is written.
// Pulling the plugin into a native test would drag in NimBLE, SD, and
// the full EventBus, which is way more than this 2-multiply formula
// deserves — so we test the math directly. A regression here is a
// regression in the plugin behaviour by construction.

#include <unity.h>
#include <cmath>

void setUp(void) {}
void tearDown(void) {}

// Same expression the plugin uses (single-precision, identity-by-default).
static inline float applyCalibration(float t_in, float slope, float offset) {
    return slope * t_in + offset;
}

// ─── Identity calibration ───────────────────────────────────

void test_identity_returns_input() {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 25.0f, applyCalibration(25.0f, 1.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f,  applyCalibration(0.0f,  1.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 100.0f, applyCalibration(100.0f, 1.0f, 0.0f));
}

// ─── Offset-only ────────────────────────────────────────────
// User reading 22 °C on a thermometer while the sensor says 20 → b=2.

void test_offset_only_shifts_uniformly() {
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 22.0f, applyCalibration(20.0f, 1.0f, 2.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 27.0f, applyCalibration(25.0f, 1.0f, 2.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 60.0f, applyCalibration(62.0f, 1.0f, -2.0f));
}

// ─── Slope + offset (two-point calibration) ─────────────────
// Reference points: at 25 °C sensor reads 24, at 60 °C sensor reads 58.5.
// Two-point fit: slope = 35/34.5 ≈ 1.01449, offset = 25 - slope*24 ≈ 0.6522.
// Sanity-check both anchor points and one intermediate (boil at 100 °C).

void test_two_point_anchors_recover() {
    const float slope  = 35.0f / 34.5f;
    const float offset = 25.0f - slope * 24.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, applyCalibration(24.0f,  slope, offset));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 60.0f, applyCalibration(58.5f,  slope, offset));
}

void test_two_point_extrapolates_linearly() {
    const float slope  = 35.0f / 34.5f;
    const float offset = 25.0f - slope * 24.0f;
    // At a (hypothetical) sensor reading of 99, the corrected output
    // should be slope*99 + offset, well-defined and finite.
    const float corrected = applyCalibration(99.0f, slope, offset);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 101.0f, corrected);  // ~101.13 °C
}

// ─── Defensive bounds (matches CommandHandler::REQ_SETTINGS_CAL_SET) ──
// The firmware rejects slope outside [0.8, 1.2] and offset outside
// ±10 °C. These tests just verify that the math doesn't blow up at the
// edges of the allowed range — it's a regression net for anyone who
// later tightens or relaxes the bounds.

void test_extreme_slope_bounds_do_not_overflow() {
    TEST_ASSERT_TRUE(std::isfinite(applyCalibration(100.0f, 0.8f, 0.0f)));
    TEST_ASSERT_TRUE(std::isfinite(applyCalibration(100.0f, 1.2f, 0.0f)));
}

void test_extreme_offset_bounds_do_not_overflow() {
    TEST_ASSERT_TRUE(std::isfinite(applyCalibration(25.0f, 1.0f, -10.0f)));
    TEST_ASSERT_TRUE(std::isfinite(applyCalibration(25.0f, 1.0f,  10.0f)));
}

// ─── Main ───────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_identity_returns_input);
    RUN_TEST(test_offset_only_shifts_uniformly);
    RUN_TEST(test_two_point_anchors_recover);
    RUN_TEST(test_two_point_extrapolates_linearly);
    RUN_TEST(test_extreme_slope_bounds_do_not_overflow);
    RUN_TEST(test_extreme_offset_bounds_do_not_overflow);
    return UNITY_END();
}
