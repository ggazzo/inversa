#include "Components/Settings.h"

#define P_ON_M 0
#define P_ON_E 1

#define HYSTERESIS_DEGREES_C_KEY "h_d_c"
#define HYSTERESIS_SECONDS_KEY "h_s"
Settings::Settings() {
    load();
}


void Settings::load() {
    preferences.begin(PREFERENCES_KEY, false);

    kp = preferences.getFloat("kp", 2.0);
    ki = preferences.getFloat("ki", 5.0);
    kd = preferences.getFloat("kd", 1.0);
    pOn = preferences.getFloat("pOn", P_ON_M);
    time = preferences.getFloat("time", 1000);
    volumeLiters = preferences.getInt("volume_liters", 70);
    powerWatts = preferences.getInt("power_watts", 3200);
    hysteresisDegreesC = preferences.getFloat(HYSTERESIS_DEGREES_C_KEY, 1);
    hysteresisSeconds = preferences.getFloat(HYSTERESIS_SECONDS_KEY, 10);
    wifiSsid = preferences.getString("wifiSsid", "");
    wifiPassword = preferences.getString("wifiPassword", "");

    preferences.end();
}

// Save settings to preferences
void Settings::save() {

    if(preferences.begin(PREFERENCES_KEY, false)){
        Serial.println("Saving settings");
    } else {
        Serial.println("Failed to open preferences");
    }
    preferences.putFloat("kp", kp);
    preferences.putFloat("ki", ki);
    preferences.putFloat("kd", kd);
    preferences.putFloat("pOn", pOn);
    preferences.putFloat("time", time);
    preferences.putInt("volumeLiters", volumeLiters);
    preferences.putInt("powerWatts", powerWatts);
    preferences.putFloat(HYSTERESIS_DEGREES_C_KEY, hysteresisDegreesC);
    preferences.putFloat(HYSTERESIS_SECONDS_KEY, hysteresisSeconds);
    preferences.putString("wifiSsid", wifiSsid);
    preferences.putString("wifiPassword", wifiPassword);
    isDirty = false;

    Serial.println("Settings saved");   

    preferences.end();
}



// Getters
float Settings::getKp() { return kp; }
float Settings::getKi() { return ki; }
float Settings::getKd() { return kd; }
float Settings::getPOn() { return pOn; }
float Settings::getTime() { return time; }
float Settings::getVolumeLiters() { return volumeLiters; }
float Settings::getPowerWatts() { return powerWatts; }
float Settings::getHysteresisDegreesC() { return hysteresisDegreesC; }
float Settings::getHysteresisSeconds() { return hysteresisSeconds; }
String Settings::getWifiSsid() { return wifiSsid; }
String Settings::getWifiPassword() { return wifiPassword; }

// Setters
void Settings::setWifiSsid(String wifiSsid) { 
    if(this->wifiSsid != wifiSsid) {
        this->wifiSsid = wifiSsid;
        isDirty = true;
    }
}
void Settings::setWifiPassword(String wifiPassword) { 
    if(this->wifiPassword != wifiPassword) {
        this->wifiPassword = wifiPassword;
        isDirty = true;
    }
}

void Settings::setKp(float kp) { 

    if(this->kp != kp) {
        this->kp = kp;
        isDirty = true;
    }
} 
void Settings::setKi(float ki) { 
    if(this->ki != ki) {
        this->ki = ki;
        isDirty = true;
    }
}
void Settings::setKd(float kd) { 
    if(this->kd != kd) {
        this->kd = kd;
        isDirty = true;
    }
}
void Settings::setPOn(float pOn) { 
    if(this->pOn != pOn) {
        this->pOn = pOn;
        isDirty = true;
    }
}
void Settings::setTime(float time) { 
    if(this->time != time) {
        this->time = time;
        isDirty = true;
    }
}
void Settings::setVolumeLiters(float volumeLiters) { 
    if(this->volumeLiters != volumeLiters) {
        this->volumeLiters = volumeLiters;
        isDirty = true;
    }
}
void Settings::setHysteresisDegreesC(float hysteresisDegreesC) { 
    if(this->hysteresisDegreesC != hysteresisDegreesC) {
        this->hysteresisDegreesC = hysteresisDegreesC;
        isDirty = true;
    }
}
void Settings::setHysteresisSeconds(float hysteresisSeconds) { 
    if(this->hysteresisSeconds != hysteresisSeconds) {
        this->hysteresisSeconds = hysteresisSeconds;
        isDirty = true;
    }
}

void Settings::setPowerWatts(float powerWatts) { 
    if(this->powerWatts != powerWatts) {
        this->powerWatts = powerWatts;
        isDirty = true;
    }
}

void Settings::clear() {
    preferences.begin(PREFERENCES_KEY, false);
    preferences.clear();
    preferences.end();
    load();
}