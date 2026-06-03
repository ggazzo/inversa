#pragma once

#include <stdint.h>

// FNV-1a (32-bit), constexpr. Shared by CommandHandler's dispatch switch
// (folds command-string hashes into case labels at compile time) and by
// test_command_table (asserts the command set is collision-free). Keeping
// one definition guarantees the test validates the exact hash the firmware
// dispatches with.
namespace hashing {
constexpr uint32_t fnv1a(const char* s) {
    uint32_t h = 0x811c9dc5u;
    while (*s) { h ^= (uint8_t)*s++; h *= 0x01000193u; }
    return h;
}
}  // namespace hashing
