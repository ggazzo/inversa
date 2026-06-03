// test_command_table.cpp — guards the BLE command dispatch table.
//
// CommandHandler dispatches by FNV-1a hash of the command `type`, then
// strcmp-verifies. The hash step is only sound if the command strings are
// collision-free. This test iterates command_table.def — the same X-macro
// list the dispatch switch is generated from — and asserts:
//
//   * no two command strings share an FNV-1a hash (dispatch precondition)
//   * no two command strings are byte-identical (a copy-paste typo guard)
//
// Coverage (every command has a handler) is enforced at compile time: a CMD
// line whose handler method doesn't exist fails to build CommandHandler.h.

#include <unity.h>
#include <cstring>
#include <cstdio>
#include "../../src/protocol/protocol.h"
#include "../../src/protocol/fnv1a.h"

// Pull the command strings out of the shared X-macro table.
static const char* const kCmds[] = {
    #define CMD(REQ, FN) Protocol::REQ,
    #include "../../src/protocol/command_table.def"
    #undef CMD
};
static const size_t kCount = sizeof(kCmds) / sizeof(kCmds[0]);

void setUp(void) {}
void tearDown(void) {}

void test_table_not_empty() {
    TEST_ASSERT_GREATER_THAN_UINT(0, kCount);
}

void test_no_fnv_collisions() {
    for (size_t i = 0; i < kCount; ++i) {
        for (size_t j = i + 1; j < kCount; ++j) {
            if (hashing::fnv1a(kCmds[i]) == hashing::fnv1a(kCmds[j])) {
                char buf[160];
                snprintf(buf, sizeof(buf),
                         "FNV-1a collision: \"%s\" and \"%s\" hash to 0x%08x",
                         kCmds[i], kCmds[j], (unsigned)hashing::fnv1a(kCmds[i]));
                TEST_FAIL_MESSAGE(buf);
            }
        }
    }
}

void test_no_duplicate_strings() {
    for (size_t i = 0; i < kCount; ++i) {
        for (size_t j = i + 1; j < kCount; ++j) {
            if (strcmp(kCmds[i], kCmds[j]) == 0) {
                char buf[120];
                snprintf(buf, sizeof(buf), "Duplicate command string: \"%s\"", kCmds[i]);
                TEST_FAIL_MESSAGE(buf);
            }
        }
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_table_not_empty);
    RUN_TEST(test_no_fnv_collisions);
    RUN_TEST(test_no_duplicate_strings);
    return UNITY_END();
}
