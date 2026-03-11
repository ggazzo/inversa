#include <unity.h>
#include "../../src/core/Semver.h"

void setUp(void) {}
void tearDown(void) {}

// ─── Basic Comparison ───────────────────────────────────────

void test_semver_equal_versions() {
    TEST_ASSERT_EQUAL(0, Semver::compare("1.0.0", "1.0.0"));
    TEST_ASSERT_EQUAL(0, Semver::compare("2.5.10", "2.5.10"));
}

void test_semver_major_comparison() {
    TEST_ASSERT_EQUAL(1, Semver::compare("2.0.0", "1.0.0"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("1.0.0", "2.0.0"));
    TEST_ASSERT_EQUAL(1, Semver::compare("10.0.0", "9.0.0"));
}

void test_semver_minor_comparison() {
    TEST_ASSERT_EQUAL(1, Semver::compare("1.1.0", "1.0.0"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("1.0.0", "1.1.0"));
    TEST_ASSERT_EQUAL(1, Semver::compare("1.10.0", "1.9.0"));
}

void test_semver_patch_comparison() {
    TEST_ASSERT_EQUAL(1, Semver::compare("1.0.1", "1.0.0"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("1.0.0", "1.0.1"));
    TEST_ASSERT_EQUAL(1, Semver::compare("1.0.10", "1.0.9"));
}

// ─── Pre-release Handling ───────────────────────────────────

void test_semver_release_beats_prerelease() {
    // Release versions are greater than pre-release versions
    TEST_ASSERT_EQUAL(1, Semver::compare("1.0.0", "1.0.0-rc.0"));
    TEST_ASSERT_EQUAL(1, Semver::compare("1.0.0", "1.0.0-alpha"));
    TEST_ASSERT_EQUAL(1, Semver::compare("1.0.0", "1.0.0-beta.1"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("1.0.0-rc.0", "1.0.0"));
}

void test_semver_prerelease_comparison() {
    TEST_ASSERT_EQUAL(1, Semver::compare("1.0.0-rc.1", "1.0.0-rc.0"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("1.0.0-rc.0", "1.0.0-rc.1"));
    TEST_ASSERT_EQUAL(0, Semver::compare("1.0.0-rc.0", "1.0.0-rc.0"));
}

void test_semver_prerelease_alphabetic() {
    // Alphabetic comparison: beta > alpha
    TEST_ASSERT_EQUAL(1, Semver::compare("1.0.0-beta.1", "1.0.0-alpha.1"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("1.0.0-alpha.1", "1.0.0-beta.1"));
}

// ─── v Prefix Handling ──────────────────────────────────────

void test_semver_v_prefix() {
    TEST_ASSERT_EQUAL(0, Semver::compare("v1.0.0", "1.0.0"));
    TEST_ASSERT_EQUAL(0, Semver::compare("1.0.0", "v1.0.0"));
    TEST_ASSERT_EQUAL(0, Semver::compare("v1.0.0", "v1.0.0"));
    TEST_ASSERT_EQUAL(1, Semver::compare("v2.0.0", "v1.0.0"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("v1.0.0", "v2.0.0"));
}

void test_semver_v_prefix_with_prerelease() {
    TEST_ASSERT_EQUAL(1, Semver::compare("v1.0.0", "v1.0.0-rc.0"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("v1.0.0-rc.0", "v1.0.0"));
    TEST_ASSERT_EQUAL(1, Semver::compare("v1.0.0-rc.1", "v1.0.0-rc.0"));
}

// ─── Invalid Versions ───────────────────────────────────────

void test_semver_invalid_versions() {
    // Invalid versions should return 0
    TEST_ASSERT_EQUAL(0, Semver::compare("1.0", "1.0.0"));
    TEST_ASSERT_EQUAL(0, Semver::compare("1.0.0", "1.0"));
    TEST_ASSERT_EQUAL(0, Semver::compare("1", "1.0.0"));
    TEST_ASSERT_EQUAL(0, Semver::compare("invalid", "1.0.0"));
}

// ─── Complex Cases ──────────────────────────────────────────

void test_semver_complex_cases() {
    // Pre-release of newer version > release of older version
    TEST_ASSERT_EQUAL(1, Semver::compare("2.0.0-rc.1", "1.9.9"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("1.9.9", "2.0.0-rc.1"));
    
    // Multi-part pre-release identifiers
    TEST_ASSERT_EQUAL(1, Semver::compare("1.0.0-rc.1.0", "1.0.0-rc.1"));
    TEST_ASSERT_EQUAL(-1, Semver::compare("1.0.0-rc.1", "1.0.0-rc.1.0"));
}

// ─── isNewer Helper ─────────────────────────────────────────

void test_semver_is_newer() {
    TEST_ASSERT_TRUE(Semver::isNewer("2.0.0", "1.0.0"));
    TEST_ASSERT_TRUE(Semver::isNewer("1.1.0", "1.0.0"));
    TEST_ASSERT_TRUE(Semver::isNewer("1.0.1", "1.0.0"));
    TEST_ASSERT_TRUE(Semver::isNewer("1.0.0", "1.0.0-rc.1"));
    
    TEST_ASSERT_FALSE(Semver::isNewer("1.0.0", "1.0.0"));
    TEST_ASSERT_FALSE(Semver::isNewer("1.0.0", "2.0.0"));
    TEST_ASSERT_FALSE(Semver::isNewer("1.0.0-rc.1", "1.0.0"));
}

// ─── Test Runner ────────────────────────────────────────────

int main(int argc, char** argv) {
    UNITY_BEGIN();
    
    // Basic comparison
    RUN_TEST(test_semver_equal_versions);
    RUN_TEST(test_semver_major_comparison);
    RUN_TEST(test_semver_minor_comparison);
    RUN_TEST(test_semver_patch_comparison);
    
    // Pre-release handling
    RUN_TEST(test_semver_release_beats_prerelease);
    RUN_TEST(test_semver_prerelease_comparison);
    RUN_TEST(test_semver_prerelease_alphabetic);
    
    // v prefix handling
    RUN_TEST(test_semver_v_prefix);
    RUN_TEST(test_semver_v_prefix_with_prerelease);
    
    // Invalid versions
    RUN_TEST(test_semver_invalid_versions);
    
    // Complex cases
    RUN_TEST(test_semver_complex_cases);
    
    // isNewer helper
    RUN_TEST(test_semver_is_newer);
    
    return UNITY_END();
}
