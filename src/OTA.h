#ifndef OTA_H
#define OTA_H

#include "Components/Base.h"

class IOTA : public Base {
    public:
        virtual void setup() = 0;
        virtual void loop() = 0;
        virtual void checkForUpdatesGithub() = 0;
};

class OTA : public IOTA {
    public:
        OTA();
        void setup() override;
        void loop() override;
        void checkForUpdatesGithub() override;

    private:
        unsigned long lastCheck;
};

#endif