#pragma once
// sim/Arduino.h — Arduino API surface for the brewing simulator (native).
// This file replaces <Arduino.h> when the firmware is built with
// `-D SIM_BUILD -I sim`. It provides just enough to make the existing
// plugins compile and run on a Linux/macOS host, with sensor and actuator
// calls routed through `ThermalSim`.

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cmath>
#include <string>
#include <algorithm>

#include "SimClock.h"
#include "ThermalSim.h"

// ─── Arduino pin macros (Arduino default SPI pin names used by board hdr) ─
#ifndef SS
#define SS    10
#endif
#ifndef SCK
#define SCK   13
#endif
#ifndef MISO
#define MISO  12
#endif
#ifndef MOSI
#define MOSI  11
#endif

// ─── Arduino constants ─────────────────────────────────────────────
#define HIGH         0x1
#define LOW          0x0
#define INPUT        0x0
#define OUTPUT       0x1
#define INPUT_PULLUP 0x2

#ifndef PI
#define PI 3.14159265358979323846
#endif

// ─── Pin assignments (firmware's HAL identifies the heater/NTC by pin) ──
// We only care about NTC and SSR; everything else is a no-op.
extern int simNtcPin;
extern int simSsrPin;
extern int simPumpPin;

inline void simBindPins(int ntc, int ssr, int pump) {
    simNtcPin  = ntc;
    simSsrPin  = ssr;
    simPumpPin = pump;
}

// ─── Time ──────────────────────────────────────────────────────────
inline uint32_t millis()  { return SimClock::nowMs(); }
inline uint32_t micros()  { return SimClock::nowUs(); }
inline void delay(uint32_t ms) { SimClock::advanceMs(ms); }
inline void delayMicroseconds(uint32_t us) {
    // Round up to 1 ms for the virtual clock.
    SimClock::advanceMs((us + 999) / 1000);
}

// ─── GPIO ──────────────────────────────────────────────────────────
inline void pinMode(int /*pin*/, int /*mode*/) {}

inline void digitalWrite(int pin, int state) {
    if (pin == simSsrPin)       ThermalSim::setHeater(state == HIGH);
    else if (pin == simPumpPin) ThermalSim::setPump(state == HIGH);
    // any other pin: silently no-op (NeoPixel, etc.)
}

inline int digitalRead(int /*pin*/) { return LOW; }

// ─── Analog ────────────────────────────────────────────────────────
inline int analogRead(int pin) {
    if (pin == simNtcPin) return ThermalSim::adcRead();
    return 0;
}
inline void analogReadResolution(int /*bits*/) {}

// ─── Math (Arduino-flavored macros — provide as functions to keep std::) ─
template <typename T> constexpr T abs_arduino(T v) { return v < 0 ? -v : v; }
#ifndef abs
#define abs(x) abs_arduino(x)
#endif

template <typename T> constexpr T min(T a, T b) { return a < b ? a : b; }
template <typename T> constexpr T max(T a, T b) { return a > b ? a : b; }

template <typename T>
inline T constrain(T x, T lo, T hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

inline long mapRange(long x, long inMin, long inMax, long outMin, long outMax) {
    return (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}
#ifndef map
#define map mapRange
#endif

// ─── String (Arduino String, backed by std::string) ────────────────
class String {
public:
    String() = default;
    String(const char* s) : _s(s ? s : "") {}
    String(const std::string& s) : _s(s) {}
    String(int v)          { char b[16]; snprintf(b, sizeof(b), "%d", v); _s = b; }
    String(unsigned int v) { char b[16]; snprintf(b, sizeof(b), "%u", v); _s = b; }
    String(long v)         { char b[24]; snprintf(b, sizeof(b), "%ld", v); _s = b; }
    String(unsigned long v){ char b[24]; snprintf(b, sizeof(b), "%lu", v); _s = b; }
    String(float v)        { char b[32]; snprintf(b, sizeof(b), "%.2f", v); _s = b; }
    String(float v, int p) { char b[32]; snprintf(b, sizeof(b), "%.*f", p, v); _s = b; }
    String(double v)       { char b[32]; snprintf(b, sizeof(b), "%.2f", v); _s = b; }

    const char* c_str() const { return _s.c_str(); }
    size_t length()    const { return _s.length(); }
    bool   isEmpty()   const { return _s.empty(); }
    char   charAt(unsigned i) const { return i < _s.length() ? _s[i] : '\0'; }
    char   operator[](unsigned i) const { return charAt(i); }

    String& operator=(const char* s)   { _s = s ? s : ""; return *this; }
    String& operator=(const String& o) = default;
    String& operator+=(const char* s)  { if (s) _s += s; return *this; }
    String& operator+=(const String& o){ _s += o._s; return *this; }
    String& operator+=(char c)         { _s += c; return *this; }
    String& operator+=(int v)          { _s += String(v)._s; return *this; }

    String  operator+(const char* s)   const { String r(*this); r += s; return r; }
    String  operator+(const String& o) const { String r(*this); r += o; return r; }
    String  operator+(char c)          const { String r(*this); r += c; return r; }
    String  operator+(int v)           const { String r(*this); r += v; return r; }

    bool operator==(const char* s)   const { return s && _s == s; }
    bool operator==(const String& o) const { return _s == o._s; }
    bool operator!=(const char* s)   const { return !(*this == s); }
    bool operator!=(const String& o) const { return !(*this == o); }

    int  indexOf(char c, int from = 0) const {
        auto p = _s.find(c, from); return p == std::string::npos ? -1 : (int)p;
    }
    int  indexOf(const char* s, int from = 0) const {
        auto p = _s.find(s ? s : "", from); return p == std::string::npos ? -1 : (int)p;
    }
    String substring(int from) const {
        if (from < 0 || (size_t)from > _s.length()) return String("");
        return String(_s.substr(from));
    }
    String substring(int from, int to) const {
        if (from < 0 || (size_t)from > _s.length()) return String("");
        if (to > (int)_s.length()) to = _s.length();
        if (to <= from) return String("");
        return String(_s.substr(from, to - from));
    }

    bool startsWith(const char* p)   const { return p && _s.rfind(p, 0) == 0; }
    bool startsWith(const String& p) const { return startsWith(p.c_str()); }
    bool endsWith(const char* sfx)   const {
        if (!sfx) return false;
        size_t n = std::strlen(sfx);
        return _s.length() >= n && _s.compare(_s.length() - n, n, sfx) == 0;
    }
    bool endsWith(const String& sfx) const { return endsWith(sfx.c_str()); }

    void trim() {
        const char* ws = " \t\r\n";
        auto a = _s.find_first_not_of(ws);
        auto b = _s.find_last_not_of(ws);
        if (a == std::string::npos) _s.clear();
        else                        _s = _s.substr(a, b - a + 1);
    }
    void toUpperCase() {
        for (auto& c : _s) c = (char)std::toupper((unsigned char)c);
    }
    void toLowerCase() {
        for (auto& c : _s) c = (char)std::tolower((unsigned char)c);
    }

    float toFloat() const {
        try { return std::stof(_s); } catch (...) { return 0.0f; }
    }
    int   toInt() const {
        try { return std::stoi(_s); } catch (...) { return 0; }
    }

    // ── Print / Stream surface (ArduinoJson uses these on String) ──
    // read()/available()/peek() are const because ArduinoJson templates
    // hand us a `const String&` for deserialization; `_readPos` is
    // therefore mutable.
    size_t write(uint8_t c)                    { _s += (char)c; return 1; }
    size_t write(const uint8_t* buf, size_t n) { if (buf) _s.append((const char*)buf, n); return n; }
    int    available() const                   { return (int)(_s.length() - _readPos); }
    int    read()      const                   { return _readPos < _s.length() ? (uint8_t)_s[_readPos++] : -1; }
    int    peek()      const                   { return _readPos < _s.length() ? (uint8_t)_s[_readPos]   : -1; }

private:
    std::string _s;
    mutable size_t _readPos = 0;
};

inline String operator+(const char* a, const String& b) { return String(a) + b; }

// ─── Serial — goes to STDERR so stdout stays clean for JSON ────────
// ─── ESP global (OTAPlugin / CommandHandler call ESP.restart, getFreeHeap) ─
class ESPClass {
public:
    void restart()           { std::exit(0); }
    uint32_t getFreeHeap()   { return 200 * 1024; }
};
extern ESPClass ESP;

class SimSerial {
public:
    void begin(unsigned long /*baud*/) {}
    void print(const char* s)   { std::fputs(s ? s : "", stderr); }
    void print(const String& s) { std::fputs(s.c_str(), stderr); }
    void print(int v)           { std::fprintf(stderr, "%d", v); }
    void print(float v)         { std::fprintf(stderr, "%.2f", v); }
    void println()              { std::fputc('\n', stderr); }
    void println(const char* s) { std::fputs(s ? s : "", stderr); std::fputc('\n', stderr); }
    void println(const String& s){ println(s.c_str()); }
    void println(int v)         { std::fprintf(stderr, "%d\n", v); }
    void println(float v)       { std::fprintf(stderr, "%.2f\n", v); }
    int  printf(const char* fmt, ...) {
        va_list ap; va_start(ap, fmt);
        int n = std::vfprintf(stderr, fmt, ap);
        va_end(ap); return n;
    }
    int available() { return 0; }
    int read()      { return -1; }
};
extern SimSerial Serial;
