#include "Settings.h"

#define P_ON_M 0
#define P_ON_E 1

Settings::Settings() {

    preferences.begin(PREFERENCES_KEY, false);
    kp = preferences.getFloat("kp", 2.0);
    ki = preferences.getFloat("ki", 5.0);
    kd = preferences.getFloat("kd", 1.0);
    pOn = preferences.getFloat("pOn", P_ON_M);
    time = preferences.getFloat("time", 1000);
    volumeLiters = preferences.getFloat("volume_liters", 70);
    powerWatts = preferences.getFloat("power_watts", 3200);
    hysteresisDegreesC = preferences.getFloat("hysteresis_degrees_c", 1);
    hysteresisSeconds = preferences.getFloat("hysteresis_seconds", 10);

    preferences.end();
}

// Save settings to preferences
void Settings::save() {
     if (!isDirty){
        return;
    }
    

    preferences.begin(PREFERENCES_KEY, false);
    preferences.putFloat("kp", kp);
    preferences.putFloat("ki", ki);
    preferences.putFloat("kd", kd);
    preferences.putFloat("pOn", pOn);
    preferences.putFloat("time", time);
    preferences.putFloat("volumeLiters", volumeLiters);
    preferences.putFloat("powerWatts", powerWatts);
    preferences.putFloat("hysteresisDegreesC", hysteresisDegreesC);
    preferences.putFloat("hysteresisSeconds", hysteresisSeconds);
    preferences.putString("wifiSsid", wifiSsid);
    preferences.putString("wifiPassword", wifiPassword);

    isDirty = false;

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