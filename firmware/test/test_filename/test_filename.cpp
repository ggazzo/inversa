// test_filename.cpp — P2 path-traversal guard for recipe filenames.
//
// Exercises the same validator the firmware uses at two layers (BLE
// dispatcher + SDCardPlugin), so a regression in either path lands here.

#include <unity.h>
#include "../test_eventbus/test_mocks.h"   // shared Arduino-String stub
#include "../../src/core/Filename.h"

void setUp(void) {}
void tearDown(void) {}

// ── Happy path ──────────────────────────────────────────────

void test_accepts_simple_name() {
    TEST_ASSERT_TRUE(Filename::isValidRecipeFilename(String("IPA.txt")));
}

void test_accepts_dashes_and_underscores() {
    TEST_ASSERT_TRUE(Filename::isValidRecipeFilename(String("my-recipe_v2.txt")));
}

void test_accepts_numbers() {
    TEST_ASSERT_TRUE(Filename::isValidRecipeFilename(String("IPA2024.txt")));
}

void test_accepts_at_max_length() {
    // 32 chars total including ".txt": 28 chars of name + ".txt"
    TEST_ASSERT_TRUE(Filename::isValidRecipeFilename(String("AAAAAAAAAAAAAAAAAAAAAAAAAAAA.txt")));
}

// ── Path traversal — the reason this validator exists ──────

void test_rejects_dotdot_traversal() {
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("../recovery.bin")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("../../etc/passwd")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("..IPA.txt")));
}

void test_rejects_absolute_paths() {
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("/etc/passwd")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("/recovery.bin")));
}

void test_rejects_path_separators() {
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("subdir/IPA.txt")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("subdir\\IPA.txt")));
}

// ── Extension enforcement ──────────────────────────────────

void test_rejects_missing_extension() {
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("recovery.bin")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA.TXT")));   // case-sensitive
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA.json")));
}

// ── Bounds ──────────────────────────────────────────────────

void test_rejects_empty() {
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("")));
}

void test_rejects_too_long() {
    // 33 chars: 29 of name + ".txt"
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("AAAAAAAAAAAAAAAAAAAAAAAAAAAAA.txt")));
}

// ── FAT-invalid characters ─────────────────────────────────

void test_rejects_invalid_chars() {
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA*.txt")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA?.txt")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA\".txt")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA<.txt")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA>.txt")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA|.txt")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA .txt")));   // space
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA:.txt")));
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String("IPA;.txt")));
}

void test_rejects_null_byte() {
    // A null in the middle would terminate Arduino String early; the mock
    // String wraps std::string which keeps the data. Either way, the byte
    // is not in the whitelist.
    char buf[] = {'I', 'P', 'A', '\x01', '.', 't', 'x', 't', 0};
    TEST_ASSERT_FALSE(Filename::isValidRecipeFilename(String(buf)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_accepts_simple_name);
    RUN_TEST(test_accepts_dashes_and_underscores);
    RUN_TEST(test_accepts_numbers);
    RUN_TEST(test_accepts_at_max_length);

    RUN_TEST(test_rejects_dotdot_traversal);
    RUN_TEST(test_rejects_absolute_paths);
    RUN_TEST(test_rejects_path_separators);

    RUN_TEST(test_rejects_missing_extension);

    RUN_TEST(test_rejects_empty);
    RUN_TEST(test_rejects_too_long);

    RUN_TEST(test_rejects_invalid_chars);
    RUN_TEST(test_rejects_null_byte);
    return UNITY_END();
}
