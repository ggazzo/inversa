#include <Arduino.h>
#include <Preferences.h>

#ifndef SETTINGS_H
#define SETTINGS_H

#define PREFERENCES_KEY "settings"

class ISettings {
    public:
        // Getters
        virtual String getWifiSsid();
        virtual String getWifiPassword();
        virtual float getKp();
        virtual float getKi();
        virtual float getKd();
        virtual float getPOn();
        virtual float getTime();
        virtual float getVolumeLiters();
        virtual float getPowerWatts();
        virtual float getHysteresisDegreesC();
        virtual float getHysteresisSeconds();

        // Setters
        virtual void setWifiSsid(String wifiSsid);
        virtual void setWifiPassword(String wifiPassword);
        virtual void setKp(float kp);
        virtual void setKi(float ki);
        virtual void setKd(float kd);
        virtual void setPOn(float pOn);
        virtual void setTime(float time);
        virtual void setVolumeLiters(float volumeLiters);
        virtual void setPowerWatts(float powerWatts);
        virtual void setHysteresisDegreesC(float hysteresisDegreesC);
        virtual void setHysteresisSeconds(float hysteresisSeconds);

        // Save settings to preferences
        virtual void save() = 0;
        virtual void clear() = 0;
        virtual void setup() = 0;
};

class Settings: public ISettings {
    public:
        Settings();

        // Getters
        String getWifiSsid() override;
        String getWifiPassword() override;
        float getKp() override;
        float getKi() override;
        float getKd() override;
        float getPOn() override;
        float getTime() override;
        float getVolumeLiters() override;
        float getPowerWatts() override;
        float getHysteresisDegreesC() override;
        float getHysteresisSeconds() override;

        // Setters
        void setWifiSsid(String wifiSsid) override;
        void setWifiPassword(String wifiPassword) override;
        void setKp(float kp) override;
        void setKi(float ki) override;
        void setKd(float kd) override;
        void setPOn(float pOn) override;
        void setTime(float time) override;
        void setVolumeLiters(float volumeLiters) override;
        void setPowerWatts(float powerWatts) override;
        void setHysteresisDegreesC(float hysteresisDegreesC) override;
        void setHysteresisSeconds(float hysteresisSeconds) override;

        virtual void save() override;
        virtual void clear() override;
        virtual void setup() override;
        

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