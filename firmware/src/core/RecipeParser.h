#pragma once

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

// ─── Recipe DSL parser ──────────────────────────────────────
// Single source of truth for the recipe command grammar, shared by
// RecipePlugin (firmware runtime) and the native unit test. Kept free of
// plugin/MachineState dependencies so the test exercises the REAL parser
// instead of a copy that silently drifts.
//
// `String` is supplied by the includer — Arduino String on device, the
// lightweight stub on the native test — so we deliberately don't include
// <Arduino.h> here.
//
// STEP only records its label here; the message→BrewingStep mapping stays in
// RecipePlugin where the BrewingStep enum lives (keeps this header dep-free).

enum class RecipeCommandType : uint8_t {
    // Temperature
    SetTemp,        // SET_TEMP <value>
    WaitTemp,       // WAIT_TEMP [tolerance]
    MashOut,        // MASH_OUT [temp]
    // Timers
    WaitTimer,      // WAIT_TIMER <minutes> - internal recipe timer
    Timer,          // TIMER <minutes> - TimerPlugin (with notifications)
    Alarm,          // ALARM <HH:MM> - absolute timer (requires RTC)
    // Boil
    Boil,           // BOIL <minutes>
    AddHop,         // ADD_HOP <minutes> "name"
    // Ramp
    Ramp,           // RAMP <rate>
    RampOff,        // RAMP_OFF
    // Actuators
    PumpOn, PumpOff, HeaterOn, HeaterOff,
    // Flow control
    WaitConfirm,    // WAIT_CONFIRM ["message"]
    WaitBoil,       // WAIT_BOIL
    // UI
    Step,           // STEP <name>
    // Other
    Comment,        // # comment / blank (ignored)
    Unknown
};

struct RecipeCommand {
    RecipeCommandType type = RecipeCommandType::Unknown;
    float value = 0;
    float value2 = 0;      // second value (e.g. minute for ALARM)
    String message = "";
};

namespace RecipeParser {

// Case-insensitive prefix match (kw must be uppercase ASCII). Callers must
// still put longer prefixes first (RAMP_OFF before RAMP).
inline bool startsWithI(const char* s, const char* kw) {
    while (*kw) {
        char a = *s++;
        char b = *kw++;
        if (a >= 'a' && a <= 'z') a = a - 32;
        if (a != b) return false;
    }
    return true;
}

inline bool equalsI(const char* a, const char* b) {
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb) return false;
    }
    return *a == 0 && *b == 0;
}

inline float parseFloatAt(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return (float)strtod(s, nullptr);
}

// First quoted "..." segment, or the right-trimmed rest if unquoted.
inline String extractQuoted(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    const char* q1 = strchr(s, '"');
    if (q1) {
        const char* q2 = strchr(q1 + 1, '"');
        if (!q2) return String(q1 + 1);
        return String(q1 + 1).substring(0, (int)(q2 - q1 - 1));
    }
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t')) n--;
    String out;
    out.concat(s, n);
    return out;
}

// Parse one line into a RecipeCommand. `waitTempDefault` is the WAIT_TEMP
// tolerance used when the line omits it. Blank/`#` lines → Comment;
// unrecognized verbs → Unknown (caller decides how to surface that).
inline RecipeCommand parseLine(const char* line, float waitTempDefault = 0.5f) {
    RecipeCommand cmd;
    while (*line == ' ' || *line == '\t') line++;

    if (*line == '#' || *line == 0) {
        cmd.type = RecipeCommandType::Comment;
        return cmd;
    }

    if (startsWithI(line, "SET_TEMP")) {
        cmd.type = RecipeCommandType::SetTemp;
        cmd.value = parseFloatAt(line + 8);
    }
    else if (startsWithI(line, "WAIT_TEMP")) {
        cmd.type = RecipeCommandType::WaitTemp;
        float tol = parseFloatAt(line + 9);
        cmd.value = (tol > 0) ? tol : waitTempDefault;
    }
    else if (startsWithI(line, "MASH_OUT")) {
        cmd.type = RecipeCommandType::MashOut;
        float temp = parseFloatAt(line + 8);
        cmd.value = (temp > 0) ? temp : 76.0f;  // default 76°C
    }
    else if (startsWithI(line, "WAIT_TIMER")) {
        cmd.type = RecipeCommandType::WaitTimer;
        cmd.value = parseFloatAt(line + 10);
    }
    else if (startsWithI(line, "TIMER")) {
        cmd.type = RecipeCommandType::Timer;
        cmd.value = parseFloatAt(line + 5);
    }
    else if (startsWithI(line, "ALARM")) {
        cmd.type = RecipeCommandType::Alarm;
        const char* p = line + 5;
        while (*p == ' ' || *p == '\t') p++;
        char* end = nullptr;
        long hour = strtol(p, &end, 10);
        if (end && *end == ':') {
            long minute = strtol(end + 1, nullptr, 10);
            cmd.value  = (float)hour;
            cmd.value2 = (float)minute;
        }
    }
    else if (startsWithI(line, "BOIL")) {
        cmd.type = RecipeCommandType::Boil;
        cmd.value = parseFloatAt(line + 4);
    }
    else if (startsWithI(line, "ADD_HOP")) {
        cmd.type = RecipeCommandType::AddHop;
        const char* rest = line + 7;
        while (*rest == ' ' || *rest == '\t') rest++;
        char* end = nullptr;
        cmd.value = strtof(rest, &end);
        if (end && end != rest) {
            cmd.message = extractQuoted(end);
            if (cmd.message.isEmpty()) cmd.message = "Hop";
        } else {
            cmd.message = "Hop";
        }
    }
    else if (startsWithI(line, "WAIT_BOIL")) {
        cmd.type = RecipeCommandType::WaitBoil;
    }
    else if (startsWithI(line, "RAMP_OFF")) {
        cmd.type = RecipeCommandType::RampOff;
    }
    else if (startsWithI(line, "RAMP")) {
        cmd.type = RecipeCommandType::Ramp;
        cmd.value = parseFloatAt(line + 4);
    }
    else if (startsWithI(line, "PUMP_ON")) {
        cmd.type = RecipeCommandType::PumpOn;
    }
    else if (startsWithI(line, "PUMP_OFF")) {
        cmd.type = RecipeCommandType::PumpOff;
    }
    else if (startsWithI(line, "HEATER_ON")) {
        cmd.type = RecipeCommandType::HeaterOn;
    }
    else if (startsWithI(line, "HEATER_OFF")) {
        cmd.type = RecipeCommandType::HeaterOff;
    }
    else if (startsWithI(line, "WAIT_CONFIRM")) {
        cmd.type = RecipeCommandType::WaitConfirm;
        cmd.message = extractQuoted(line + 12);
        if (cmd.message.isEmpty()) cmd.message = "Confirmar para continuar";
    }
    else if (startsWithI(line, "STEP")) {
        cmd.type = RecipeCommandType::Step;
        cmd.message = extractQuoted(line + 4);  // BrewingStep mapping in RecipePlugin
    }
    else {
        cmd.type = RecipeCommandType::Unknown;
    }

    return cmd;
}

}  // namespace RecipeParser
