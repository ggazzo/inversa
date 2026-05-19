#pragma once

#include <Arduino.h>
#include <Preferences.h>

// ─── NVS Storage ────────────────────────────────────────────
// Persistent key-value storage using ESP32's NVS (Non-Volatile Storage).
// Used for PID parameters, settings, and other persistent data.

class NVSStorage {
public:
    static NVSStorage& instance() {
        static NVSStorage inst;
        return inst;
    }

    bool begin() {
        bool ok = _prefs.begin("inversa", false);  // false = read/write mode
        if (ok) {
            DEBUG_PRINTLN("[NVS] Storage initialized");
        } else {
            DEBUG_PRINTLN("[NVS] Failed to initialize");
        }
        return ok;
    }

    void end() {
        _prefs.end();
    }

    // ── PID Parameters ──────────────────────────────────────
    void savePIDParams(float kp, float ki, float kd) {
        _prefs.putFloat("pid_kp", kp);
        _prefs.putFloat("pid_ki", ki);
        _prefs.putFloat("pid_kd", kd);
        DEBUG_PRINTF("[NVS] Saved PID: Kp=%.2f Ki=%.4f Kd=%.1f\n", kp, ki, kd);
    }

    float loadPIDKp(float defaultVal) {
        return _prefs.getFloat("pid_kp", defaultVal);
    }

    float loadPIDKi(float defaultVal) {
        return _prefs.getFloat("pid_ki", defaultVal);
    }

    float loadPIDKd(float defaultVal) {
        return _prefs.getFloat("pid_kd", defaultVal);
    }

    bool hasPIDParams() {
        return _prefs.isKey("pid_kp");
    }

    // ── WiFi Credentials ─────────────────────────────────────
    void saveWiFiCredentials(const String& ssid, const String& password) {
        _prefs.putString("wifi_ssid", ssid);
        _prefs.putString("wifi_pwd", password);
        DEBUG_PRINTF("[NVS] Saved WiFi SSID: %s\n", ssid.c_str());
    }

    String getWiFiSSID() {
        return _prefs.getString("wifi_ssid", "");
    }

    String getWiFiPassword() {
        return _prefs.getString("wifi_pwd", "");
    }

    bool hasWiFiCredentials() {
        return _prefs.isKey("wifi_ssid") && _prefs.getString("wifi_ssid", "").length() > 0;
    }

    void clearWiFiCredentials() {
        _prefs.remove("wifi_ssid");
        _prefs.remove("wifi_pwd");
        DEBUG_PRINTLN("[NVS] WiFi credentials cleared");
    }

    // ── Thermal Parameters (P13) ────────────────────────────
    // Persist vessel thermal params so feed-forward survives reboot.
    // Keys ≤15 chars (NVS limit).
    void saveThermalParams(float volumeL, float powerW, float ambientC,
                           float diameterM, float lossCoeff) {
        _prefs.putFloat("therm_vol_l",  volumeL);
        _prefs.putFloat("therm_pwr_w",  powerW);
        _prefs.putFloat("therm_amb_c",  ambientC);
        _prefs.putFloat("therm_diam_m", diameterM);
        _prefs.putFloat("therm_loss_c", lossCoeff);
        DEBUG_PRINTF("[NVS] Saved thermal: V=%.1fL P=%.0fW Tamb=%.1f D=%.2fm h=%.1f\n",
                     volumeL, powerW, ambientC, diameterM, lossCoeff);
    }

    float loadThermalVolumeL(float defaultVal)    { return _prefs.getFloat("therm_vol_l",  defaultVal); }
    float loadThermalPowerW(float defaultVal)     { return _prefs.getFloat("therm_pwr_w",  defaultVal); }
    float loadThermalAmbientC(float defaultVal)   { return _prefs.getFloat("therm_amb_c",  defaultVal); }
    float loadThermalDiameterM(float defaultVal)  { return _prefs.getFloat("therm_diam_m", defaultVal); }
    float loadThermalLossCoeff(float defaultVal)  { return _prefs.getFloat("therm_loss_c", defaultVal); }

    bool hasThermalParams() {
        return _prefs.isKey("therm_vol_l");
    }

    // ── Temperature Calibration ─────────────────────────────
    // Linear correction applied after the Kalman filter:
    //     T_real = slope · T_medido + offset
    // Default = identity (slope=1, offset=0). Keys ≤15 chars.
    void saveTempCalibration(float slope, float offset) {
        _prefs.putFloat("cal_t_slope", slope);
        _prefs.putFloat("cal_t_off",   offset);
        DEBUG_PRINTF("[NVS] Saved temp calibration: slope=%.4f offset=%.2f\n",
                     slope, offset);
    }

    float loadTempCalSlope(float defaultVal)  { return _prefs.getFloat("cal_t_slope", defaultVal); }
    float loadTempCalOffset(float defaultVal) { return _prefs.getFloat("cal_t_off",   defaultVal); }

    bool hasTempCalibration() {
        return _prefs.isKey("cal_t_slope");
    }

    // ── RTC / Timezone (P14) ────────────────────────────────
    // UTC offset in minutes — supports half-hour zones (e.g., India = 330).
    // Default −180 = UTC-3 (Brazil; matches previous hardcoded behavior).
    void saveTimezoneOffsetMin(int16_t minutes) {
        _prefs.putInt("rtc_tz_min", (int32_t)minutes);
        DEBUG_PRINTF("[NVS] Saved timezone offset: %d min\n", minutes);
    }

    int16_t loadTimezoneOffsetMin(int16_t defaultVal = -180) {
        return (int16_t)_prefs.getInt("rtc_tz_min", defaultVal);
    }

    bool hasTimezoneOffset() {
        return _prefs.isKey("rtc_tz_min");
    }

    // ── Thermal Watchdog (001-thermal-watchdog) ─────────────
    // Persist watchdog config, trip statistics, and the latched flag.
    // Keys ≤15 chars (NVS limit). All keys use prefix `wd_*`.
    void saveWatchdogConfig(float hardStopC, uint32_t sensorFaultMs, uint32_t loopStuckMs,
                            uint8_t gradFactor, uint8_t gradWindow, float safeAutoresetC,
                            uint32_t coolMinMs, bool autoResetEnabled) {
        _prefs.putFloat ("wd_cfg_hs",  hardStopC);
        _prefs.putUInt  ("wd_cfg_sfm", sensorFaultMs);
        _prefs.putUInt  ("wd_cfg_lsm", loopStuckMs);
        _prefs.putUChar ("wd_cfg_gf",  gradFactor);
        _prefs.putUChar ("wd_cfg_gw",  gradWindow);
        _prefs.putFloat ("wd_cfg_sa",  safeAutoresetC);
        _prefs.putUInt  ("wd_cfg_cm",  coolMinMs);
        _prefs.putBool  ("wd_cfg_are", autoResetEnabled);
        DEBUG_PRINTF("[NVS] Saved watchdog config: hs=%.1f sfm=%u lsm=%u gf=%u gw=%u sa=%.1f cm=%u are=%d\n",
                     hardStopC, sensorFaultMs, loopStuckMs, gradFactor, gradWindow,
                     safeAutoresetC, coolMinMs, (int)autoResetEnabled);
    }

    float    loadWatchdogHardStopC       (float    defaultVal) { return _prefs.getFloat("wd_cfg_hs",  defaultVal); }
    uint32_t loadWatchdogSensorFaultMs   (uint32_t defaultVal) { return _prefs.getUInt ("wd_cfg_sfm", defaultVal); }
    uint32_t loadWatchdogLoopStuckMs     (uint32_t defaultVal) { return _prefs.getUInt ("wd_cfg_lsm", defaultVal); }
    uint8_t  loadWatchdogGradFactor      (uint8_t  defaultVal) { return _prefs.getUChar("wd_cfg_gf",  defaultVal); }
    uint8_t  loadWatchdogGradWindow      (uint8_t  defaultVal) { return _prefs.getUChar("wd_cfg_gw",  defaultVal); }
    float    loadWatchdogSafeAutoresetC  (float    defaultVal) { return _prefs.getFloat("wd_cfg_sa",  defaultVal); }
    uint32_t loadWatchdogCoolMinMs       (uint32_t defaultVal) { return _prefs.getUInt ("wd_cfg_cm",  defaultVal); }
    bool     loadWatchdogAutoResetEnabled(bool     defaultVal) { return _prefs.getBool ("wd_cfg_are", defaultVal); }

    bool hasWatchdogConfig() {
        return _prefs.isKey("wd_cfg_hs");
    }

    // Trip stats — written on each transition armed→tripped.
    void saveWatchdogTrip(uint32_t count, uint8_t cause, uint32_t unixTs) {
        _prefs.putUInt ("wd_trip_cnt", count);
        _prefs.putUChar("wd_trip_lc",  cause);
        _prefs.putUInt ("wd_trip_lu",  unixTs);
        DEBUG_PRINTF("[NVS] Saved watchdog trip: count=%u cause=%u unix=%u\n",
                     count, cause, unixTs);
    }

    uint32_t loadWatchdogTripCount   (uint32_t defaultVal) { return _prefs.getUInt ("wd_trip_cnt", defaultVal); }
    uint8_t  loadWatchdogLastCause   (uint8_t  defaultVal) { return _prefs.getUChar("wd_trip_lc",  defaultVal); }
    uint32_t loadWatchdogLastTripUnix(uint32_t defaultVal) { return _prefs.getUInt ("wd_trip_lu",  defaultVal); }

    // Latched flag — survives reboot so heater stays inhibited after crash.
    void saveWatchdogLatched(bool latched) {
        _prefs.putBool("wd_latched", latched);
    }

    bool loadWatchdogLatched(bool defaultVal) {
        return _prefs.getBool("wd_latched", defaultVal);
    }

    // Explicit clear (preserves count/cause/unix for diagnostics audit).
    void clearWatchdogLatched() {
        _prefs.putBool("wd_latched", false);
        DEBUG_PRINTLN("[NVS] Watchdog latched cleared");
    }

    // ── Factory Reset (P7 — guarded clear) ──────────────────
    // Apaga todo o namespace `inversa`. Caller deve confirmar com magic
    // string antes de chamar; ver CommandHandler::handleFactoryReset.
    bool factoryReset(const String& confirm) {
        if (confirm != "ERASE_ALL") {
            DEBUG_PRINTLN("[NVS] factoryReset rejected — missing confirm");
            return false;
        }
        _prefs.clear();
        DEBUG_PRINTLN("[NVS] Factory reset — namespace 'inversa' cleared");
        return true;
    }

    // ── Generic Methods ─────────────────────────────────────
    void putFloat(const char* key, float value) {
        _prefs.putFloat(key, value);
    }

    float getFloat(const char* key, float defaultVal) {
        return _prefs.getFloat(key, defaultVal);
    }

    void putInt(const char* key, int32_t value) {
        _prefs.putInt(key, value);
    }

    int32_t getInt(const char* key, int32_t defaultVal) {
        return _prefs.getInt(key, defaultVal);
    }

    void putBool(const char* key, bool value) {
        _prefs.putBool(key, value);
    }

    bool getBool(const char* key, bool defaultVal) {
        return _prefs.getBool(key, defaultVal);
    }

    void putString(const char* key, const String& value) {
        _prefs.putString(key, value);
    }

    String getString(const char* key, const String& defaultVal) {
        return _prefs.getString(key, defaultVal);
    }

    bool hasKey(const char* key) {
        return _prefs.isKey(key);
    }

    void remove(const char* key) {
        _prefs.remove(key);
    }

    void clear() {
        _prefs.clear();
    }

private:
    NVSStorage() = default;
    Preferences _prefs;
};
