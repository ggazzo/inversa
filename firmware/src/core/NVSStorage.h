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
