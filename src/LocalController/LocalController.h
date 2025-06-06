#ifndef LOCAL_CONTROLLER_H
#define LOCAL_CONTROLLER_H
#include <RTClib.h>
#include <NTPClient.h>
#include "ESP32FtpServer.h"

#include "Controller/PeripheralController.h"
#include "Controller/MainController.h"

#include "PowerRecovery.h"

#include "state.h"

class LocalController : public MainController<StateType, Steps> {
    public:
        LocalController(StateMachine *task, Settings *settings, CommunicationPeripherals *communicationPeripherals, RTC_DS1307 *rtc, MachineState *state, PeripheralController *peripheralController);
        void setTargetTemperature(float target_temperature_c) override;
        void setTargetTemperatureAndWait(float target_temperature_c) override;
        void prepareTemperature(float targetTemperature_celsius, unsigned long desiredTime_minutes_from_now_seconds) override;
        void prepareTemperature(float targetTemperature_celsius, char* desiredTime_hhmm_ss) override;
        void waitConfirmation() override;
        void setState(StateType state) override;

        void confirm() override;
        void abort() override;
        void skip() override;

        void setup() override;
        void loop() override;

        float getTemperature() override;
        float getTargetTemperature() override;

        float getVolume() override;
        float getPower() override;
        float getHysteresisDegreesC() override;
        float getHysteresisSeconds() override;

        void startAutotune(float targetTemperature, int samples) override;
        void stopAutotune() override;

        void startTotalTimeCounter() override;
        unsigned long getTimeStart() override;
        void startTotalTimeCounter(unsigned long start_time_seconds) override;
        void stopTotalTimeCounter() override;
        void resetTotalTimeCounter() override;
        void setEstimatedTime(unsigned long estimatedTime_seconds) override;
        unsigned long getEstimatedTime() override;

        unsigned long getElapsedTime() override;
        unsigned long remainingTime() override;

        void startStepTimeCounter() override;
        void startStepTimeCounter(unsigned long start_time_seconds) override;
        void stopStepTimeCounter() override;
        unsigned long getStepTimeStart() override;
        unsigned long getStepElapsedTime() override;
        void setStepEstimatedTime(unsigned long estimatedTime_seconds) override;

        void waitForStepTime() override;
        void checkForUpdates() override;

        void waitForTimer(unsigned long duration_seconds) override;
        void stopTimer() override;
        bool isTimeFinished() override;

        void saveMilestoneToPowerLoss() override;
        void deleteMilestoneFromPowerLoss() override;

        StateType getState() override;
        String getTimeString() override;
        void setTime(char* isoDate) override;

        MachineState *state;
    private:
        RTC_DS1307 *rtc;
        NTPClient timeClient;
        PeripheralController *peripheralController;

        Steps currentStep;
        FtpServer ftpSrv;


        unsigned long totalTimeStart = 0;
        unsigned long estimatedTime = 0;

        unsigned long stepTimeStart = 0;
        unsigned long stepEstimatedTime = 0;


        PowerRecovery<MachineState> powerRecovery;



        void restoreStateFromPowerLoss();

        uint32_t now();

};

#endif
