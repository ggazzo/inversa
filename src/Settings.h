#include <Arduino.h>
#include <Preferences.h>

#ifndef SETTINGS_H
#define SETTINGS_H

#define PREFERENCES_KEY "settings"

class Settings {
    public:
        Settings();

        // Getters
        String getWifiSsid();
        String getWifiPassword();
        float getKp();
        float getKi();
        float getKd();
        float getPOn();
        float getTime();
        float getVolumeLiters();
        float getPowerWatts();
        float getHysteresisDegreesC();
        float getHysteresisSeconds();

        // Setters
        void setWifiSsid(String wifiSsid);
        void setWifiPassword(String wifiPassword);
        void setKp(float kp);
        void setKi(float ki);
        void setKd(float kd);
        void setPOn(float pOn);
        void setTime(float time);
        void setVolumeLiters(float volumeLiters);
        void setPowerWatts(float powerWatts);
        void setHysteresisDegreesC(float hysteresisDegreesC);
        void setHysteresisSeconds(float hysteresisSeconds);

        // Save settings to preferences
        virtual void save();

        virtual void load();
        

    private:
        Preferences preferences;

        bool isDirty = false;

        String wifiSsid = "";
        String wifiPassword = "";
        float kp = 2;
        float ki = 5;
        float kd = 1;
        float pOn = 1;
        int time = 1000;
        int volumeLiters = 70;
        int powerWatts = 3200;
        int hysteresisDegreesC = 1;
        int hysteresisSeconds = 10;
};

#endif