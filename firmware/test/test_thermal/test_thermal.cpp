// test_thermal.cpp — Pure-math unit tests for core/ThermalCalc.h.
//
// ThermalCalc is the physics layer the PID feed-forward (P13) and Scheduler
// "be ready at" feature rely on. If any of these formulas drift, both
// features misbehave — Scheduler arrives late and PID overshoots/undershoots.

#include <unity.h>
#include <cmath>
#include "../../src/core/ThermalCalc.h"

void setUp(void) {}
void tearDown(void) {}

// ─── Cylinder surface area ──────────────────────────────────

// 20 L cylinder, 35 cm diameter → height = 0.02 / (π * 0.175²) ≈ 0.208 m
//   side = 2π * 0.175 * 0.208 ≈ 0.2285 m²
//   bottom = π * 0.175² ≈ 0.0962 m²
//   total ≈ 0.3247 m²
void test_cylinder_surface_20L_35cm() {
    float area = ThermalCalc::cylinderSurfaceArea(20.0f, 0.35f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3247f, area);
}

// For a fixed diameter, more volume means a taller vessel and thus more
// side area. (Same-volume / different-diameter cylinders have a U-shaped
// total area curve so the comparison is not monotone — keep this test on
// the simpler "more water → more surface" axis.)
void test_cylinder_surface_larger_volume_increases_area() {
    float small = ThermalCalc::cylinderSurfaceArea(10.0f, 0.35f);
    float big   = ThermalCalc::cylinderSurfaceArea(40.0f, 0.35f);
    TEST_ASSERT_TRUE(big > small);
}

// ─── Heat loss (Newton's cooling, convection only) ──────────

// Q = h * A * ΔT.  For h=10, A=0.3 m², ΔT=40°C → Q = 120 W.
void test_heat_loss_basic() {
    float q = ThermalCalc::calculateHeatLoss(60.0f, 20.0f, 0.30f, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 120.0f, q);
}

void test_heat_loss_zero_at_equilibrium() {
    float q = ThermalCalc::calculateHeatLoss(25.0f, 25.0f, 0.30f, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, q);
}

// ─── Heating time ───────────────────────────────────────────

// 20 L water, 3000 W heater, 90% efficiency, 25→65°C, no loss.
// Energy = 20 * 4186 * 40 = 3,348,800 J;  P_eff = 3000 * 0.9 = 2700 W
// Time = 3,348,800 / 2700 ≈ 1240 s ≈ 20.7 min
void test_heating_time_no_loss() {
    float t = ThermalCalc::calculateHeatingTime(20.0f, 3000.0f, 25.0f, 65.0f, 0.90f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 1240.0f, t);
}

void test_heating_time_target_below_current_returns_zero() {
    float t = ThermalCalc::calculateHeatingTime(20.0f, 3000.0f, 70.0f, 65.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t);
}

void test_heating_time_negative_when_loss_exceeds_power() {
    // 100 W heater can't overcome a 200 W loss — returns -1 sentinel.
    float t = ThermalCalc::calculateHeatingTime(20.0f, 100.0f, 25.0f, 65.0f, 0.90f, 200.0f);
    TEST_ASSERT_TRUE(t < 0);
}

// ─── Heating time with dynamic loss ─────────────────────────

// Sanity: with realistic loss, time should be > no-loss baseline.
void test_heating_time_with_loss_is_slower() {
    float surfaceArea = ThermalCalc::cylinderSurfaceArea(20.0f, 0.35f);
    float t_lossy = ThermalCalc::calculateHeatingTimeWithLoss(
        20.0f, 3000.0f, 25.0f, 65.0f, 20.0f, surfaceArea, 0.90f, 10.0f);
    float t_ideal = ThermalCalc::calculateHeatingTime(20.0f, 3000.0f, 25.0f, 65.0f, 0.90f, 0.0f);
    TEST_ASSERT_TRUE(t_lossy > t_ideal);
}

// ─── Feed-forward output (PID P13 path) ─────────────────────

// targetTemp ≤ ambient → only loss term, which is also ≈ 0 → low FF output.
void test_feedforward_low_at_ambient() {
    float area = ThermalCalc::cylinderSurfaceArea(20.0f, 0.35f);
    float ff = ThermalCalc::calculateFeedForward(
        25.0f, 25.0f, 3000.0f, 20.0f, 25.0f, area, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, ff);
}

// Output is clamped to [0, 1].
void test_feedforward_clamped_to_unit() {
    float area = ThermalCalc::cylinderSurfaceArea(20.0f, 0.35f);
    // Tiny heater + huge demand → would exceed 1, must clamp.
    float ff = ThermalCalc::calculateFeedForward(
        100.0f, 20.0f, 50.0f, 200.0f, 20.0f, area, 10.0f);
    TEST_ASSERT_TRUE(ff >= 0.0f);
    TEST_ASSERT_TRUE(ff <= 1.0f);
}

// Higher delta-T → higher FF output (monotonic in the warm-up region).
// For a 20 L pot with a 3 kW heater the linear region saturates fast — we
// need either a tiny pot or a huge heater to land below the 1.0 clamp.
void test_feedforward_increases_with_error() {
    float area = ThermalCalc::cylinderSurfaceArea(20.0f, 0.35f);
    // 30 kW maxPower keeps both samples in the unsaturated region.
    float ff_warm = ThermalCalc::calculateFeedForward(65.0f, 60.0f, 30000.0f, 20.0f, 25.0f, area, 10.0f);
    float ff_cold = ThermalCalc::calculateFeedForward(65.0f, 30.0f, 30000.0f, 20.0f, 25.0f, area, 10.0f);
    TEST_ASSERT_TRUE(ff_cold > ff_warm);
    TEST_ASSERT_TRUE(ff_warm < 1.0f && ff_cold < 1.0f);   // both unsaturated
}

// ─── Minimum power to hold setpoint ─────────────────────────

void test_minimum_power_matches_loss() {
    float area = 0.3f;
    float minP = ThermalCalc::calculateMinimumPower(60.0f, 20.0f, area, 10.0f);
    float loss = ThermalCalc::calculateHeatLoss(60.0f, 20.0f, area, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, loss, minP);
}

// ─── Cooling time (Newton's law of cooling) ─────────────────

// Cooling 90→80°C in 20 L with reasonable surface should take some
// positive time. The exact value depends on integration but should be
// well below an hour (3600 s) and above zero.
void test_cooling_time_positive_and_bounded() {
    float area = ThermalCalc::cylinderSurfaceArea(20.0f, 0.35f);
    float t = ThermalCalc::calculateCoolingTime(90.0f, 80.0f, 25.0f, 20.0f, area, 10.0f);
    TEST_ASSERT_TRUE(t > 0.0f);
    TEST_ASSERT_TRUE(t < 7200.0f);  // 2h ceiling sanity
}

void test_cooling_time_zero_if_target_above_current() {
    float area = 0.3f;
    float t = ThermalCalc::calculateCoolingTime(50.0f, 60.0f, 25.0f, 20.0f, area, 10.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, t);
}

// ─── Main ───────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_cylinder_surface_20L_35cm);
    RUN_TEST(test_cylinder_surface_larger_volume_increases_area);
    RUN_TEST(test_heat_loss_basic);
    RUN_TEST(test_heat_loss_zero_at_equilibrium);
    RUN_TEST(test_heating_time_no_loss);
    RUN_TEST(test_heating_time_target_below_current_returns_zero);
    RUN_TEST(test_heating_time_negative_when_loss_exceeds_power);
    RUN_TEST(test_heating_time_with_loss_is_slower);
    RUN_TEST(test_feedforward_low_at_ambient);
    RUN_TEST(test_feedforward_clamped_to_unit);
    RUN_TEST(test_feedforward_increases_with_error);
    RUN_TEST(test_minimum_power_matches_loss);
    RUN_TEST(test_cooling_time_positive_and_bounded);
    RUN_TEST(test_cooling_time_zero_if_target_above_current);
    return UNITY_END();
}
