#pragma once

#ifdef NATIVE_BUILD
  // Native unit tests provide a `String` stub via test_mocks.h, which the
  // test source includes before this header.
#else
  #include <Arduino.h>
#endif

// ─── Filename Validation (P2) ───────────────────────────────
// Whitelist-based check used at the BLE dispatcher and again inside
// SDCardPlugin (defense in depth). Extracted into its own header so it can
// be exercised by the native test runner without dragging in SD/SPI.

namespace Filename {

constexpr size_t MAX_RECIPE_FILENAME_LEN = 32;

// Recipe filenames must:
//   - be 1..MAX chars
//   - end in ".txt"
//   - contain only [A-Za-z0-9._-]
//   - never contain ".." (defense against /recipes/../recovery.bin etc.)
inline bool isValidRecipeFilename(const String& name) {
    if (name.length() == 0 || name.length() > MAX_RECIPE_FILENAME_LEN) return false;
    if (!name.endsWith(".txt")) return false;
    if (name.indexOf("..") >= 0) return false;
    for (size_t i = 0; i < name.length(); i++) {
        char c = name.charAt(i);
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

}  // namespace Filename
