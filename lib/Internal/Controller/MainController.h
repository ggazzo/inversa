#ifndef MAIN_CONTROLLER_H
#define MAIN_CONTROLLER_H

#include "Controller.h"
#include "Components/Settings.h"
#include "Communication/CommunicationPeripherals.h"

#include "StateMachine.h"

template <typename T, typename S>
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

        virtual void startAutotune(float targetTemperature, int samples) = 0;
        virtual void stopAutotune() = 0;
        virtual T getState() = 0;
        virtual S getStep() {
            return this->step;
        }

        virtual void setStep(S step) {
            this->step = step;
            this->stopStepTimeCounter();
        }

        virtual void startTotalTimeCounter() = 0;
        virtual void startTotalTimeCounter(unsigned long start_time_seconds) = 0;
        virtual void stopTotalTimeCounter() = 0;
        virtual void resetTotalTimeCounter() = 0;
        virtual unsigned long getTimeStart() = 0;
        virtual unsigned long getElapsedTime() = 0;
        virtual void setEstimatedTime(unsigned long estimatedTime_seconds) = 0;
        virtual unsigned long getEstimatedTime() = 0;


        virtual void startStepTimeCounter() = 0;
        virtual void startStepTimeCounter(unsigned long start_time_seconds) = 0;
        virtual void stopStepTimeCounter() = 0;
        virtual unsigned long getStepTimeStart() = 0;
        virtual unsigned long getStepElapsedTime() = 0;
        virtual void setStepEstimatedTime(unsigned long estimatedTime_seconds) = 0;

        virtual void waitForStepTime() = 0;
        virtual void waitForTimer(unsigned long duration_seconds) = 0;
        virtual void stopTimer() = 0;
        virtual bool isTimeFinished() = 0;

        virtual void saveMilestoneToPowerLoss() = 0;
        virtual void deleteMilestoneFromPowerLoss() = 0;

        virtual String getTimeString() = 0;
        virtual void setTime(char* isoDate) = 0;


    protected:
        S step;
        StateMachine *task;
        Settings *settings;
        CommunicationPeripherals *communicationPeripherals;

};

#endif
