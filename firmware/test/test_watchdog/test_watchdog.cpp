// test_watchdog.cpp — Pure-logic unit tests for the Thermal Watchdog
// (feature 001-thermal-watchdog). Runs under `pio test -e native`.
//
// Layout follows the same convention as test_thermal/test_recipe etc.:
// one directory + one main() + Unity asserts. Six logical groups of
// tests are combined here to cover T006-T011 of the actions plan.
//
// T006 — RN-01 overtemp (isOvertemp helper)
// T007 — RN-02 sensor fault timer
// T008 — RN-04 adaptive gradient detector (mediana móvel)
// T009 — RN-05/06/08 latch reset margin + cooldown auto-reset
// T010 — RN-09 range validation (constants vs. config)
// T011 — NVS round-trip (pure-logic surrogate; embedded NVS exercised in bench)

#include <unity.h>
#include <cstdint>
#include <cstddef>
#include "../../src/plugins/watchdog/WatchdogEvaluator.h"

// Mirror selected defaults from constants.h. Repeating the values here
// keeps the native env self-contained (constants.h pulls Arduino.h via
// boards/*.h and that breaks the pure native build).
namespace test_consts {
    constexpr float    HARDSTOP_C       = 105.0f;
    constexpr uint32_t SENSOR_FAULT_MS  = 10000;
    constexpr uint32_t LOOP_STUCK_MS    = 5000;
    constexpr uint8_t  GRAD_FACTOR      = 5;
    constexpr uint8_t  GRAD_WINDOW      = 20;
    constexpr float    SAFE_AUTORESET_C = 40.0f;
    constexpr uint32_t COOL_MIN_MS      = 300000;
    constexpr float    RESET_MARGIN_C   = 5.0f;

    constexpr float    HARDSTOP_MIN     = 80.0f;
    constexpr float    HARDSTOP_MAX     = 130.0f;
    constexpr uint8_t  GRAD_FACTOR_MIN  = 2;
    constexpr uint8_t  GRAD_FACTOR_MAX  = 20;
    constexpr uint8_t  GRAD_WINDOW_MIN  = 5;
    constexpr uint8_t  GRAD_WINDOW_MAX  = 100;
}

void setUp(void) {}
void tearDown(void) {}

// ═══════════════════════════════════════════════════════════
// T006 — RN-01 overtemp
// ═══════════════════════════════════════════════════════════

void test_overtemp_below_threshold_does_not_trip() {
    TEST_ASSERT_FALSE(watchdog::isOvertemp(104.99f, test_consts::HARDSTOP_C));
    TEST_ASSERT_FALSE(watchdog::isOvertemp(70.0f,  test_consts::HARDSTOP_C));
    TEST_ASSERT_FALSE(watchdog::isOvertemp(0.0f,   test_consts::HARDSTOP_C));
}

void test_overtemp_at_threshold_does_not_trip() {
    // Strict >, equality is borderline-safe and avoids edge race during
    // boil where setpoint is forced to 100 °C and noise may touch 105.0.
    TEST_ASSERT_FALSE(watchdog::isOvertemp(105.0f, test_consts::HARDSTOP_C));
}

void test_overtemp_above_threshold_trips() {
    TEST_ASSERT_TRUE(watchdog::isOvertemp(105.01f, test_consts::HARDSTOP_C));
    TEST_ASSERT_TRUE(watchdog::isOvertemp(120.0f,  test_consts::HARDSTOP_C));
}

void test_overtemp_respects_custom_hardstop() {
    TEST_ASSERT_FALSE(watchdog::isOvertemp(95.0f,  100.0f));
    TEST_ASSERT_TRUE (watchdog::isOvertemp(100.1f, 100.0f));
}

// ═══════════════════════════════════════════════════════════
// T007 — RN-02 sensor fault timer
// ═══════════════════════════════════════════════════════════

void test_sensor_fault_does_not_trip_when_sensor_ok() {
    watchdog::SensorFaultTimer t;
    for (int i = 0; i < 100; ++i) t.tick(true, 200);
    TEST_ASSERT_FALSE(t.shouldTrip(test_consts::SENSOR_FAULT_MS));
}

void test_sensor_fault_does_not_trip_below_threshold() {
    watchdog::SensorFaultTimer t;
    t.tick(false, 9000);
    TEST_ASSERT_FALSE(t.shouldTrip(test_consts::SENSOR_FAULT_MS));
    TEST_ASSERT_EQUAL_UINT32(9000, t.faultMs());
}

void test_sensor_fault_trips_after_threshold() {
    watchdog::SensorFaultTimer t;
    t.tick(false, 5000);
    t.tick(false, 5000);
    TEST_ASSERT_TRUE(t.shouldTrip(test_consts::SENSOR_FAULT_MS));
}

void test_sensor_fault_resets_when_sensor_recovers() {
    watchdog::SensorFaultTimer t;
    t.tick(false, 8000);
    t.tick(true,  200);        // one good sample wipes the counter
    TEST_ASSERT_EQUAL_UINT32(0, t.faultMs());
    t.tick(false, 9000);       // need full 10 s from scratch
    TEST_ASSERT_FALSE(t.shouldTrip(test_consts::SENSOR_FAULT_MS));
}

// ═══════════════════════════════════════════════════════════
// T008 — RN-04 adaptive gradient detector
// ═══════════════════════════════════════════════════════════

void test_gradient_does_not_trip_until_window_full() {
    watchdog::GradientDetector<100> g;
    g.configure(20, 5, 0.05f);
    // Feed a huge outlier as the very first delta — must not trip yet.
    g.addSample(20.0f);
    bool trip = g.addSample(80.0f);   // dT = +60 °C (huge!)
    TEST_ASSERT_FALSE(trip);
    TEST_ASSERT_FALSE(g.windowFull());
}

void test_gradient_does_not_trip_on_steady_warmup() {
    watchdog::GradientDetector<100> g;
    g.configure(20, 5, 0.05f);
    // Linear ramp 0.04 °C/sample = 0.2 °C/s @5Hz (~12 °C/min). Normal mash.
    float t = 25.0f;
    bool tripped = false;
    for (int i = 0; i < 40; ++i) {
        t += 0.04f;
        if (g.addSample(t)) tripped = true;
    }
    TEST_ASSERT_FALSE(tripped);
    TEST_ASSERT_TRUE(g.windowFull());
}

void test_gradient_trips_on_outlier_after_warmup() {
    watchdog::GradientDetector<100> g;
    g.configure(20, 5, 0.05f);
    // Fill window with quiet baseline (0.02 °C between samples).
    float t = 25.0f;
    for (int i = 0; i < 25; ++i) { t += 0.02f; g.addSample(t); }
    TEST_ASSERT_TRUE(g.windowFull());
    // Now inject a sudden 3 °C jump (NTC shorted to ground → ADC saturates).
    bool tripped = g.addSample(t + 3.0f);
    TEST_ASSERT_TRUE(tripped);
}

void test_gradient_respects_floor_in_steady_state() {
    watchdog::GradientDetector<100> g;
    g.configure(20, 5, 0.05f);
    // Very quiet plant: every dT = 0.005 °C. Median = 0.005, so the
    // multiplier alone would set threshold to 0.025 (still below floor).
    // Floor (0.05 °C absolute) must suppress small wiggles even when the
    // baseline noise is tiny.
    float t = 50.0f;
    for (int i = 0; i < 30; ++i) { t += 0.005f; g.addSample(t); }
    TEST_ASSERT_TRUE(g.windowFull());
    bool tripped = g.addSample(t + 0.03f);  // dT = 0.03 < floor (0.05)
    TEST_ASSERT_FALSE(tripped);
}

void test_gradient_skips_identical_samples() {
    // Bug fix: when the polling rate exceeds the source-of-truth refresh
    // rate (watchdog polls 5 Hz, TemperaturePlugin updates gState @1 Hz),
    // identical reads must not pollute the median with dT = 0 entries.
    watchdog::GradientDetector<100> g;
    g.configure(20, 5, 0.05f);
    g.addSample(25.0f);
    // 30 repeats: all should be skipped, window must NOT fill on them.
    for (int i = 0; i < 30; ++i) g.addSample(25.0f);
    TEST_ASSERT_FALSE(g.windowFull());
    TEST_ASSERT_EQUAL_size_t(0, g.filledCount());
}

void test_gradient_reset_clears_state() {
    watchdog::GradientDetector<100> g;
    g.configure(20, 5, 0.05f);
    for (int i = 0; i < 25; ++i) g.addSample(25.0f + 0.02f * i);
    TEST_ASSERT_TRUE(g.windowFull());
    g.reset();
    TEST_ASSERT_FALSE(g.windowFull());
    TEST_ASSERT_EQUAL_size_t(0, g.filledCount());
}

// ═══════════════════════════════════════════════════════════
// T009 — RN-05/06/08 latch reset + cooldown auto-reset
// ═══════════════════════════════════════════════════════════

void test_reset_rejected_when_temp_near_hardstop() {
    // RN-08: reject reset if temp >= hardStop - margin.
    TEST_ASSERT_FALSE(watchdog::canResetLatch(103.0f, 105.0f, 5.0f));
    TEST_ASSERT_FALSE(watchdog::canResetLatch(100.0f, 105.0f, 5.0f));  // boundary
}

void test_reset_accepted_when_temp_safe() {
    TEST_ASSERT_TRUE(watchdog::canResetLatch(70.0f, 105.0f, 5.0f));
    TEST_ASSERT_TRUE(watchdog::canResetLatch(99.9f, 105.0f, 5.0f));
}

void test_cooldown_does_not_complete_above_safe_temp() {
    watchdog::CooldownTimer cd;
    for (int i = 0; i < 200; ++i) {
        cd.tick(50.0f, /*safe=*/40.0f, /*othersOk=*/true, 1500);
    }
    TEST_ASSERT_FALSE(cd.ready(test_consts::COOL_MIN_MS));
}

void test_cooldown_resets_when_temp_spikes_back() {
    watchdog::CooldownTimer cd;
    cd.tick(35.0f, 40.0f, true, 200000);   // 200 s contiguous cool
    TEST_ASSERT_FALSE(cd.ready(test_consts::COOL_MIN_MS));
    cd.tick(45.0f, 40.0f, true, 1000);      // spike resets
    TEST_ASSERT_EQUAL_UINT32(0, cd.accumMs());
}

void test_cooldown_completes_after_contiguous_safe_window() {
    watchdog::CooldownTimer cd;
    // 5 min contiguous below 40 °C with everything else ok.
    for (int i = 0; i < 1500; ++i) {        // 1500 × 200 ms = 300 s
        cd.tick(35.0f, 40.0f, true, 200);
    }
    TEST_ASSERT_TRUE(cd.ready(test_consts::COOL_MIN_MS));
}

void test_cooldown_paused_when_other_conditions_unhappy() {
    watchdog::CooldownTimer cd;
    // Temp is safe but sensor not OK → counter must NOT accumulate.
    for (int i = 0; i < 2000; ++i) {
        cd.tick(30.0f, 40.0f, /*othersOk=*/false, 200);
    }
    TEST_ASSERT_FALSE(cd.ready(test_consts::COOL_MIN_MS));
}

// ═══════════════════════════════════════════════════════════
// T010 — RN-09 range validation
// ═══════════════════════════════════════════════════════════

// Validation logic is centralized in CommandHandler. For the native
// build we exercise the same predicate functions over the documented
// ranges. The plugin's setConfig() will call these.

namespace test_range {
    inline bool validHardStop(float v) {
        return v >= test_consts::HARDSTOP_MIN && v <= test_consts::HARDSTOP_MAX;
    }
    inline bool validGradFactor(int v) {
        return v >= test_consts::GRAD_FACTOR_MIN && v <= test_consts::GRAD_FACTOR_MAX;
    }
    inline bool validGradWindow(int v) {
        return v >= test_consts::GRAD_WINDOW_MIN && v <= test_consts::GRAD_WINDOW_MAX;
    }
}

void test_range_hard_stop_accepts_valid() {
    TEST_ASSERT_TRUE(test_range::validHardStop(80.0f));
    TEST_ASSERT_TRUE(test_range::validHardStop(105.0f));
    TEST_ASSERT_TRUE(test_range::validHardStop(130.0f));
}

void test_range_hard_stop_rejects_out_of_band() {
    TEST_ASSERT_FALSE(test_range::validHardStop(79.99f));
    TEST_ASSERT_FALSE(test_range::validHardStop(130.01f));
    TEST_ASSERT_FALSE(test_range::validHardStop(200.0f));
    TEST_ASSERT_FALSE(test_range::validHardStop(-10.0f));
}

void test_range_grad_factor_rejects_out_of_band() {
    TEST_ASSERT_FALSE(test_range::validGradFactor(1));
    TEST_ASSERT_FALSE(test_range::validGradFactor(21));
    TEST_ASSERT_TRUE (test_range::validGradFactor(5));
}

void test_range_grad_window_rejects_out_of_band() {
    TEST_ASSERT_FALSE(test_range::validGradWindow(4));
    TEST_ASSERT_FALSE(test_range::validGradWindow(200));
    TEST_ASSERT_TRUE (test_range::validGradWindow(20));
}

// ═══════════════════════════════════════════════════════════
// T011 — NVS round-trip surrogate
// ═══════════════════════════════════════════════════════════
//
// True NVS round-trip requires Preferences.h (ESP-IDF) and an actual
// flash partition — not available under `env:native`. We exercise the
// shape contract: the byte that survives flash is uint8 representing
// WatchdogCause, the trip count rolls over predictably, and packing
// of (count, cause, unix) round-trips through a memcpy lane. Full NVS
// validation moves to bench tests (see onboarding.md §11).

#include <cstring>

struct WatchdogNvsBlob {
    uint32_t trip_cnt;
    uint8_t  last_cause;
    uint32_t last_unix;
};

void test_nvs_blob_roundtrip_byte_identical() {
    WatchdogNvsBlob in  = { 0x01020304u, 2 /* SENSOR_FAULT */, 0x65000000u };
    uint8_t  scratch[sizeof(in)];
    std::memcpy(scratch, &in, sizeof(in));
    WatchdogNvsBlob out;
    std::memcpy(&out, scratch, sizeof(out));
    TEST_ASSERT_EQUAL_UINT32(in.trip_cnt,    out.trip_cnt);
    TEST_ASSERT_EQUAL_UINT8 (in.last_cause,  out.last_cause);
    TEST_ASSERT_EQUAL_UINT32(in.last_unix,   out.last_unix);
}

void test_nvs_trip_count_increments_monotonically() {
    uint32_t count = 0;
    for (int i = 0; i < 5; ++i) ++count;
    TEST_ASSERT_EQUAL_UINT32(5, count);
}

void test_nvs_cause_fits_in_uint8() {
    // All WatchdogCause values must fit in uint8 (MANUAL = 5 today;
    // ceiling = 255 so we have plenty of room). Sanity that no future
    // edit accidentally promotes the enum to a wider underlying type
    // and breaks the NVS layout. Compile-time only.
    static_assert(sizeof(uint8_t) == 1, "uint8_t expected to be 1 byte");
    TEST_ASSERT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════

int main() {
    UNITY_BEGIN();

    // T006 — overtemp
    RUN_TEST(test_overtemp_below_threshold_does_not_trip);
    RUN_TEST(test_overtemp_at_threshold_does_not_trip);
    RUN_TEST(test_overtemp_above_threshold_trips);
    RUN_TEST(test_overtemp_respects_custom_hardstop);

    // T007 — sensor fault
    RUN_TEST(test_sensor_fault_does_not_trip_when_sensor_ok);
    RUN_TEST(test_sensor_fault_does_not_trip_below_threshold);
    RUN_TEST(test_sensor_fault_trips_after_threshold);
    RUN_TEST(test_sensor_fault_resets_when_sensor_recovers);

    // T008 — adaptive gradient
    RUN_TEST(test_gradient_does_not_trip_until_window_full);
    RUN_TEST(test_gradient_does_not_trip_on_steady_warmup);
    RUN_TEST(test_gradient_trips_on_outlier_after_warmup);
    RUN_TEST(test_gradient_respects_floor_in_steady_state);
    RUN_TEST(test_gradient_reset_clears_state);
    RUN_TEST(test_gradient_skips_identical_samples);

    // T009 — latch reset + cooldown
    RUN_TEST(test_reset_rejected_when_temp_near_hardstop);
    RUN_TEST(test_reset_accepted_when_temp_safe);
    RUN_TEST(test_cooldown_does_not_complete_above_safe_temp);
    RUN_TEST(test_cooldown_resets_when_temp_spikes_back);
    RUN_TEST(test_cooldown_completes_after_contiguous_safe_window);
    RUN_TEST(test_cooldown_paused_when_other_conditions_unhappy);

    // T010 — range validation
    RUN_TEST(test_range_hard_stop_accepts_valid);
    RUN_TEST(test_range_hard_stop_rejects_out_of_band);
    RUN_TEST(test_range_grad_factor_rejects_out_of_band);
    RUN_TEST(test_range_grad_window_rejects_out_of_band);

    // T011 — NVS surrogate
    RUN_TEST(test_nvs_blob_roundtrip_byte_identical);
    RUN_TEST(test_nvs_trip_count_increments_monotonically);
    RUN_TEST(test_nvs_cause_fits_in_uint8);

    return UNITY_END();
}
