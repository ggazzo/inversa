#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>
#include "../core/Plugin.h"
#include "../core/EventBus.h"
#include "../core/constants.h"
#include "../models/MachineState.h"
#include "WiFiPlugin.h"

// ─── RTC Plugin ─────────────────────────────────────────────
// Manages real-time clock for accurate timekeeping.
// - Uses DS1307 hardware RTC via I2C
// - Syncs with NTP when WiFi is available
// - Provides unix timestamps for timers and scheduling

class RTCPlugin : public Plugin {
public:
    RTCPlugin(MachineState& state, WiFiPlugin& wifi) 
        : _state(state), _wifi(wifi) {}

    const char* getName() const override { return "RTC"; }

    bool setup() override {
        // Initialize I2C with board-specific pins
        Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
        
        // Try to initialize RTC
        if (_rtc.begin()) {
            _rtcAvailable = true;
            
            // Check if RTC lost power and needs to be set
            if (!_rtc.isrunning()) {
                DEBUG_PRINTLN("[RTC] RTC lost power, setting to compile time");
                _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            }
            
            _state.rtcAvailable = true;
            _state.rtcTimestamp = now();
            DEBUG_PRINTF("[RTC] Initialized. Current time: %lu\n", now());
        } else {
            _rtcAvailable = false;
            _state.rtcAvailable = false;
            DEBUG_PRINTLN("[RTC] DS1307 not found!");
        }

        // Subscribe to WiFi connected event for NTP sync
        bus().subscribe(EventType::WiFiConnected, [this](const Event& e) {
            DEBUG_PRINTLN("[RTC] WiFi connected, scheduling NTP sync");
            _ntpSyncPending = true;
        });

        return true;
    }

    void loop() override {
        uint32_t currentMillis = millis();

        // Update state timestamp periodically
        if (currentMillis - _lastUpdate >= 1000) {
            _lastUpdate = currentMillis;
            if (_rtcAvailable) {
                _state.rtcTimestamp = now();
            }
        }

        // NTP sync when WiFi available
        if (_ntpSyncPending && _wifi.isConnected()) {
            if (currentMillis - _lastNtpAttempt >= NTP_RETRY_INTERVAL_MS) {
                _lastNtpAttempt = currentMillis;
                syncNTP();
            }
        }

        // Periodic NTP sync (every hour)
        if (_rtcAvailable && _wifi.isConnected() && _lastNtpSuccess > 0) {
            if (currentMillis - _lastNtpSuccess >= NTP_SYNC_INTERVAL_MS) {
                syncNTP();
            }
        }
    }

    // ── Public API ───────────────────────────────────────────

    // Get current unix timestamp
    uint32_t now() {
        if (_rtcAvailable) {
            return _rtc.now().unixtime();
        }
        // Fallback: use millis() offset (less accurate)
        return _bootTimestamp + (millis() / 1000);
    }

    // Get DateTime object
    DateTime getDateTime() {
        if (_rtcAvailable) {
            return _rtc.now();
        }
        return DateTime(now());
    }

    // Set time from unix timestamp
    void setTime(uint32_t unixTimestamp) {
        if (_rtcAvailable) {
            _rtc.adjust(DateTime(unixTimestamp));
            _state.rtcTimestamp = unixTimestamp;
            DEBUG_PRINTF("[RTC] Time set to: %lu\n", unixTimestamp);
            bus().publish(EventType::RTCTimeUpdated, (float)unixTimestamp);
        }
    }

    // Set time from ISO string (YYYY-MM-DDTHH:MM:SS)
    void setTimeFromISO(const char* isoDate) {
        if (_rtcAvailable) {
            DateTime dt(isoDate);
            _rtc.adjust(dt);
            _state.rtcTimestamp = dt.unixtime();
            DEBUG_PRINTF("[RTC] Time set from ISO: %s\n", isoDate);
            bus().publish(EventType::RTCTimeUpdated, (float)dt.unixtime());
        }
    }

    // Sync time from NTP server
    void syncNTP() {
        if (!_wifi.isConnected()) {
            DEBUG_PRINTLN("[RTC] Cannot sync NTP - WiFi not connected");
            return;
        }

        DEBUG_PRINTLN("[RTC] Syncing with NTP...");
        
        // Use configTime for ESP32 built-in NTP
        configTime(NTP_UTC_OFFSET_SEC, 0, NTP_SERVER);
        
        // Wait for time to be set (with timeout)
        uint32_t start = millis();
        struct tm timeinfo;
        while (!getLocalTime(&timeinfo) && (millis() - start < 5000)) {
            delay(100);
        }

        if (getLocalTime(&timeinfo)) {
            // Convert to unix timestamp and set RTC
            time_t now_t = mktime(&timeinfo);
            setTime((uint32_t)now_t);
            
            _ntpSyncPending = false;
            _lastNtpSuccess = millis();
            _state.rtcNtpSynced = true;
            
            DEBUG_PRINTF("[RTC] NTP sync successful: %04d-%02d-%02d %02d:%02d:%02d\n",
                        timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            DEBUG_PRINTLN("[RTC] NTP sync failed");
        }
    }

    // Format current time as string (HH:MM:SS)
    String getTimeString() {
        DateTime dt = getDateTime();
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", dt.hour(), dt.minute(), dt.second());
        return String(buf);
    }

    // Format current date as string (YYYY-MM-DD)
    String getDateString() {
        DateTime dt = getDateTime();
        char buf[12];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", dt.year(), dt.month(), dt.day());
        return String(buf);
    }

    bool isAvailable() const { return _rtcAvailable; }
    bool isNtpSynced() const { return _state.rtcNtpSynced; }

private:
    static constexpr const char* NTP_SERVER = "pool.ntp.org";
    static constexpr int32_t NTP_UTC_OFFSET_SEC = -3 * 3600;  // UTC-3 (Brazil)
    static constexpr uint32_t NTP_SYNC_INTERVAL_MS = 3600000;  // 1 hour
    static constexpr uint32_t NTP_RETRY_INTERVAL_MS = 30000;   // 30 seconds

    MachineState& _state;
    WiFiPlugin& _wifi;
    RTC_DS1307 _rtc;
    
    bool _rtcAvailable = false;
    bool _ntpSyncPending = false;
    uint32_t _lastUpdate = 0;
    uint32_t _lastNtpAttempt = 0;
    uint32_t _lastNtpSuccess = 0;
    uint32_t _bootTimestamp = 0;  // Fallback if no RTC
};
