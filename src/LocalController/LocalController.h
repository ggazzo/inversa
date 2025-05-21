#ifndef LOCAL_CONTROLLER_H
#define LOCAL_CONTROLLER_H
#include <RTClib.h>
#include <NTPClient.h>

#include "Controller/PeripheralController.h"
#include "Controller/MainController.h"


#include "state.h"

class LocalController : public MainController<StateType> {
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

        void startAutotune() override;
        void stopAutotune() override;

        StateType getState() override;


        MachineState *state;
    private:
        RTC_DS1307 *rtc;
        NTPClient timeClient;
        PeripheralController *peripheralController;
};

#endif
