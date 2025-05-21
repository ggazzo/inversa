#include <RTClib.h>
#include <WiFiUdp.h>

const long utcOffsetInSeconds = - 3 * 60 * 60;

#include "LocalController.h"
#include "States/stateMachine.h"

WiFiUDP ntpUDP;

LocalController::LocalController(StateMachine *task, Settings *settings, CommunicationPeripherals *communicationPeripherals, RTC_DS1307 *rtc, MachineState *state, PeripheralController *peripheralController): MainController<StateType>(task, settings, communicationPeripherals), rtc(rtc), timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds), state(state), peripheralController(peripheralController) {}

void LocalController::confirm() {
    Serial.println("LocalController::confirm");
}

void LocalController::setTargetTemperature(float target_temperature_c) {
    this->communicationPeripherals->setTargetTemperature(target_temperature_c);
}

void LocalController::setTargetTemperatureAndWait(float target_temperature_c) {
    this->setTargetTemperature(target_temperature_c);
    this->task->setState(&waitForTemperatureState);
}

void LocalController::abort() {
    this->task->setState(&idleState);
}

void LocalController::skip() {
    this->task->setState(&idleState);
}

void LocalController::prepareTemperature(float targetTemperature_celsius, unsigned long desiredTime_minutes_from_now_seconds) {

    preparingState.volume_liters = settings->getVolumeLiters();
    preparingState.power_watts = settings->getPowerWatts();
    preparingState.target_temperature_c = targetTemperature_celsius;

    preparingState.time_seconds = desiredTime_minutes_from_now_seconds;

    // #ifdef USE_RTC 
    //     state.target_preparing_time_seconds = rtc->now().secondstime() + preparingState.time_seconds;
    // #endif


    this->task->setState(&preparingState);
}

void LocalController::prepareTemperature(float targetTemperature_celsius, char* desiredTime_hhmm_ss) {
    this->prepareTemperature(targetTemperature_celsius, (DateTime(desiredTime_hhmm_ss).secondstime() - this->rtc->now().secondstime()));
}

void LocalController::setup() {
    MainController::setup();
    peripheralController->setup();
    if (rtc->begin()) {
        if (!rtc->isrunning()) {
            timeClient.begin();
            if(timeClient.update()){
                rtc->adjust(DateTime(timeClient.getEpochTime()));
            }
            else {
                rtc->adjust(DateTime(F(__DATE__), F(__TIME__)));
            }
        }
    }
}

void LocalController::loop() {
    MainController::loop();
    peripheralController->loop();
}

void LocalController::waitConfirmation() {
    this->task->setState(&confirmState);
}

float LocalController::getTemperature() {
    return this->state->current_temperature_c;
}

float LocalController::getVolume() {
    return this->settings->getVolumeLiters();
}

float LocalController::getPower() {
    return this->settings->getPowerWatts();
}

StateType LocalController::getState() {
    return this->state->current;
}

void LocalController::setState(StateType state) {
    this->state->current = state;
}

float LocalController::getTargetTemperature() {
    return this->state->target_temperature_c;
}

float LocalController::getHysteresisDegreesC() {
    return this->settings->getHysteresisDegreesC();
}

float LocalController::getHysteresisSeconds() {
    return this->settings->getHysteresisSeconds();
}