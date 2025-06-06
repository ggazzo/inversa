#include <unity.h>
#include "semver.h"

void test_semver_basic_comparison() {
    TEST_ASSERT_EQUAL(0, compareSemVer("1.0.0", "1.0.0"));
    TEST_ASSERT_EQUAL(1, compareSemVer("2.0.0", "1.0.0"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.0.0", "2.0.0"));
    TEST_ASSERT_EQUAL(1, compareSemVer("1.1.0", "1.0.0"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.0.0", "1.1.0"));
    TEST_ASSERT_EQUAL(1, compareSemVer("1.0.1", "1.0.0"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.0.0", "1.0.1"));
}

void test_semver_pre_release() {
    // Pre-release versions are less than release versions
    TEST_ASSERT_EQUAL(1, compareSemVer("1.0.0", "1.0.0-rc.0"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.0.0-rc.0", "1.0.0"));
    
    // Pre-release comparison
    TEST_ASSERT_EQUAL(1, compareSemVer("1.0.0-rc.1", "1.0.0-rc.0"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.0.0-rc.0", "1.0.0-rc.1"));
    TEST_ASSERT_EQUAL(0, compareSemVer("1.0.0-rc.0", "1.0.0-rc.0"));
    
    // Different pre-release types
    TEST_ASSERT_EQUAL(1, compareSemVer("1.0.0-beta.1", "1.0.0-alpha.1"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.0.0-alpha.1", "1.0.0-beta.1"));
}

void test_semver_invalid_versions() {
    // Invalid versions should return 0
    TEST_ASSERT_EQUAL(0, compareSemVer("1.0", "1.0.0"));
    TEST_ASSERT_EQUAL(0, compareSemVer("1.0.0", "1.0"));
    TEST_ASSERT_EQUAL(0, compareSemVer("1", "1.0.0"));
    TEST_ASSERT_EQUAL(0, compareSemVer("1.0.0", "1"));
    TEST_ASSERT_EQUAL(0, compareSemVer("1.0.0-", "1.0.0"));
    TEST_ASSERT_EQUAL(0, compareSemVer("1.0.0", "1.0.0-"));
}

void test_semver_complex_cases() {
    // Complex version comparisons
    TEST_ASSERT_EQUAL(1, compareSemVer("2.0.0-rc.1", "1.9.9"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.9.9", "2.0.0-rc.1"));
    TEST_ASSERT_EQUAL(1, compareSemVer("1.0.0-rc.2", "1.0.0-rc.1"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.0.0-rc.1", "1.0.0-rc.2"));
    TEST_ASSERT_EQUAL(1, compareSemVer("1.0.0-rc.1.0", "1.0.0-rc.1"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.0.0-rc.1", "1.0.0-rc.1.0"));
}

void test_semver_prefix() {
    // Test versions with 'v' prefix
    TEST_ASSERT_EQUAL(0, compareSemVer("v1.0.0", "1.0.0"));
    TEST_ASSERT_EQUAL(0, compareSemVer("1.0.0", "v1.0.0"));
    TEST_ASSERT_EQUAL(0, compareSemVer("v1.0.0", "v1.0.0"));
    TEST_ASSERT_EQUAL(1, compareSemVer("v2.0.0", "v1.0.0"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("v1.0.0", "v2.0.0"));
    
    // Test pre-release with prefix
    TEST_ASSERT_EQUAL(1, compareSemVer("v1.0.0", "v1.0.0-rc.0"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("v1.0.0-rc.0", "v1.0.0"));
    TEST_ASSERT_EQUAL(1, compareSemVer("v1.0.0-rc.1", "v1.0.0-rc.0"));
}

void test_semver_dirty() {
    TEST_ASSERT_EQUAL(1, compareSemVer("1.0.0-rc.0", "1.0.0-rc.0+dirty"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("1.0.0-rc.0+dirty", "1.0.0-rc.0"));
    TEST_ASSERT_EQUAL(1, compareSemVer("v1.0.0-rc.0+dirty", "v1.0.0-rc.0"));
    TEST_ASSERT_EQUAL(-1, compareSemVer("v1.0.0-rc.0", "v1.0.0-rc.0+dirty"));
}

void RUN_UNITY_TESTS() {
    UNITY_BEGIN();
    RUN_TEST(test_semver_basic_comparison);
    RUN_TEST(test_semver_pre_release);
    RUN_TEST(test_semver_invalid_versions);
    RUN_TEST(test_semver_complex_cases);
    RUN_TEST(test_semver_prefix);
    UNITY_END();
}

int main(int argc, char **argv) {
    RUN_UNITY_TESTS();
    return 0;
} 