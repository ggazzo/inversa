#ifndef MAIN_CONTROLLER_H
#define MAIN_CONTROLLER_H

#include "Controller.h"
#include "Components/Settings.h"
#include "Communication/CommunicationPeripherals.h"

#include "StateMachine.h"

template <typename T>
class MainController : public Controller {
    public:
        MainController(StateMachine *task, Settings *settings, CommunicationPeripherals *communicationPeripherals):
            task(task),
            settings(settings),
            communicationPeripherals(communicationPeripherals) {
        }

        ~MainController() {
        }

        void loop() override {
            this->task->run();
        }

        void setup() override {
            this->settings->load();
            this->communicationPeripherals->setPidParameters(this->settings->getKp(), this->settings->getKi(), this->settings->getKd(), this->settings->getPOn(), this->settings->getTime());
        }

        virtual void confirm() = 0;
        virtual void abort() = 0;
        virtual void skip() = 0;

        virtual void waitConfirmation() = 0;
        virtual void setTargetTemperature(float target_temperature_c) = 0;
        virtual void setTargetTemperatureAndWait(float target_temperature_c) = 0;
        virtual void prepareTemperature(float targetTemperature_celsius, unsigned long desiredTime_minutes_from_now_seconds) = 0;
        virtual void prepareTemperature(float targetTemperature_celsius, char* desiredTime_hhmm_ss) = 0;
        virtual void setState(T state) = 0;


        virtual float getHysteresisDegreesC() = 0;
        virtual float getHysteresisSeconds() = 0;
        virtual float getTemperature() = 0;
        virtual float getTargetTemperature() = 0;
        virtual float getVolume() = 0;
        virtual float getPower() = 0;

        virtual void startAutotune() = 0;
        virtual void stopAutotune() = 0;
        virtual T getState() = 0;

    protected:
        StateMachine *task;
        Settings *settings;
        CommunicationPeripherals *communicationPeripherals;
};

#endif
