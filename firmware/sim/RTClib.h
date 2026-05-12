#pragma once
// RTClib.h stub — virtual RTC tied to SimClock.
// SimClock starts at 0; we expose an epoch offset so wall-time arithmetic
// (Scheduler "be ready at HH:MM") produces sensible results.

#include "Arduino.h"

class DateTime {
public:
    DateTime() = default;
    DateTime(uint32_t epochSec)           : _u(epochSec) {}
    DateTime(int y, int mo, int d, int h, int mi, int s)
        : _u(epochFromYmdHms(y, mo, d, h, mi, s)) {}
    DateTime(const char* /*date*/, const char* /*time*/) : _u(0) {}
    DateTime(const class __FlashStringHelper*, const class __FlashStringHelper*) : _u(0) {}
    DateTime(const char* /*iso*/)         : _u(0) {}

    uint32_t unixtime() const { return _u; }
    int year()   const { return 2026; }
    int month()  const { return 5; }
    int day()    const { return 12; }
    int hour()   const { return (int)((_u / 3600) % 24); }
    int minute() const { return (int)((_u / 60)   % 60); }
    int second() const { return (int)( _u         % 60); }

private:
    static uint32_t epochFromYmdHms(int /*y*/, int /*mo*/, int /*d*/, int h, int mi, int s) {
        return (uint32_t)h * 3600 + (uint32_t)mi * 60 + (uint32_t)s;
    }
    uint32_t _u = 0;
};

// Allow the firmware to do `DateTime(F(__DATE__), F(__TIME__))`. We don't
// need a real flash-string helper here.
class __FlashStringHelper {};
#ifndef F
#define F(x) ((const __FlashStringHelper*)(x))
#endif

class RTC_DS1307 {
public:
    bool begin()                     { return true; }
    bool isrunning()                 { return true; }
    void adjust(const DateTime& dt)  { _epoch = dt.unixtime(); }
    DateTime now()                   { return DateTime(_epoch + SimClock::nowMs() / 1000); }

private:
    uint32_t _epoch = 1747008000u;   // 2025-05-12T00:00:00Z arbitrary start
};

#include <time.h>
// ESP32 wall-clock helpers used by RTCPlugin's NTP path. Always fail so
// `syncNTP` returns gracefully on the simulator.
inline void configTime(long /*tz*/, long /*dst*/, const char* /*server*/) {}
inline bool getLocalTime(struct tm*) { return false; }
