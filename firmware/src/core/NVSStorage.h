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
            Serial.println("[NVS] Storage initialized");
        } else {
            Serial.println("[NVS] Failed to initialize");
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
        Serial.printf("[NVS] Saved PID: Kp=%.2f Ki=%.4f Kd=%.1f\n", kp, ki, kd);
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
        Serial.printf("[NVS] Saved WiFi SSID: %s\n", ssid.c_str());
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
        Serial.println("[NVS] WiFi credentials cleared");
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
