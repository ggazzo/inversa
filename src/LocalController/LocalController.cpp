#include <RTClib.h>
#include <WiFiUdp.h>

#include "OTA.h"

#include "LocalController.h"
#include "States/stateMachine.h"
#include "api.h"


// ============================================================================
// CONSTRUCTOR AND CORE SETUP
// ============================================================================

LocalController::LocalController(StateMachine *task, ISettings *settings, CommunicationPeripherals *communicationPeripherals, IRTC *rtc, MachineState *state, PeripheralController *peripheralController, IAPI *api): MainController<StateType, Steps>(task, settings, communicationPeripherals), rtc(rtc), state(state), peripheralController(peripheralController), api(api), currentStep(Steps::NONE), powerRecovery() {
    this->step = Steps::NONE;
}

void LocalController::setup() {
    MainController::setup();
    setupOTA();
    peripheralController->setup();

    this->api->setup();

    this->rtc->setup();

    this->ftpSrv.begin("esp32", "esp32");

    this->restoreStateFromPowerLoss();
}

void LocalController::loop() {
    MainController::loop();
    handleOTA(this->state->current == StateType::IDLE);
    peripheralController->loop();
    this->ftpSrv.handleFTP();
    this->rtc->loop();
    this->api->loop();
}


// ============================================================================
// STATE MANAGEMENT AND CONTROL FLOW
// ============================================================================

void LocalController::confirm() {
    Serial.println("LocalController::confirm");
    this->saveMilestoneToPowerLoss();
    this->skip();
}

void LocalController::abort() {
    this->task->setState(&idleState);
    this->stopTotalTimeCounter();
    this->stopTimer();
    this->setTargetTemperature(0);
    this->deleteMilestoneFromPowerLoss();
}

void LocalController::skip() {
    this->task->setState(&idleState);
}

void LocalController::waitConfirmation() {
    this->task->setState(&confirmState);
}

void LocalController::setState(StateType state) {
    this->state->current = state;
    if(state != StateType::IDLE){
        this->startTotalTimeCounter();
    }
    this->saveMilestoneToPowerLoss();
}

StateType LocalController::getState() {
    return this->state->current;
}

void LocalController::checkForUpdates() {
    checkForUpdatesGithub();
}

// ============================================================================
// TEMPERATURE CONTROL
// ============================================================================

void LocalController::setTargetTemperature(float target_temperature_c) {
    this->communicationPeripherals->setTargetTemperature(target_temperature_c);
    this->saveMilestoneToPowerLoss();
}

void LocalController::setTargetTemperatureAndWait(float target_temperature_c) {
    this->setTargetTemperature(target_temperature_c);
    this->task->setState(&waitForTemperatureState);
    this->saveMilestoneToPowerLoss();
}

void LocalController::prepareTemperature(float targetTemperature_celsius, unsigned long desiredTime_minutes_from_now_minutes) {

    preparingState.volume_liters = settings->getVolumeLiters();
    preparingState.power_watts = settings->getPowerWatts();
    preparingState.target_temperature_c = targetTemperature_celsius;

    this->state->target_timer_time_seconds = this->now() + desiredTime_minutes_from_now_minutes * 60;
    this->setStep(Steps::PRE_HEATING);
    this->startStepTimeCounter();
    this->setEstimatedTime(this->state->target_timer_time_seconds);
    this->task->setState(&preparingState);
    this->saveMilestoneToPowerLoss();
}

void LocalController::prepareTemperature(float targetTemperature_celsius, char* desiredTime_hhmm_ss) {
    this->prepareTemperature(targetTemperature_celsius, (DateTime(desiredTime_hhmm_ss).secondstime() - this->now()));
}

float LocalController::getTemperature() {
    return this->state->current_temperature_c;
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

void LocalController::startAutotune(float targetTemperature, int samples) {
    this->communicationPeripherals->startAutotune(targetTemperature, samples);
}

void LocalController::stopAutotune() {
    this->communicationPeripherals->stopAutotune();
}

// ============================================================================
// TIME MANAGEMENT
// ============================================================================

uint32_t LocalController::now() {
    return this->rtc->now();
}

String LocalController::getTimeString() {
    return DateTime(this->rtc->now()).timestamp();
}

void LocalController::setTime(char* isoDate) {
    this->rtc->setTime(isoDate);
}

// Total Time Management
void LocalController::startTotalTimeCounter(unsigned long start_time_seconds) {
    if(this->state->total_time_seconds_start != 0){
        return;
    }
    this->state->total_time_seconds_start = start_time_seconds;
    this->saveMilestoneToPowerLoss();
}

void LocalController::startTotalTimeCounter() {
    this->startTotalTimeCounter(this->now());
}

unsigned long LocalController::getTimeStart() {
    return this->state->total_time_seconds_start;
}

void LocalController::stopTotalTimeCounter() {
    this->state->total_time_seconds_start = 0;
    this->setEstimatedTime(0);
}

void LocalController::resetTotalTimeCounter() {
    this->state->total_time_seconds_start = 0;
    this->startTotalTimeCounter();
}

unsigned long LocalController::getElapsedTime() {
    if (this->state->total_time_seconds_start == 0) {
        return 0;
    }
    return this->now() - this->state->total_time_seconds_start;
}

void LocalController::setEstimatedTime(unsigned long estimatedTime_seconds) {
    this->state->total_time_seconds_estimated = estimatedTime_seconds;
    this->saveMilestoneToPowerLoss();
}

unsigned long LocalController::getEstimatedTime() {
    return this->state->total_time_seconds_estimated;
}

unsigned long LocalController::remainingTime() {
    return this->state->target_timer_time_seconds - this->now();
}

// Step Time Management
void LocalController::startStepTimeCounter() {
    this->startStepTimeCounter(this->now());
}

void LocalController::startStepTimeCounter(unsigned long start_time_seconds) {
    this->state->step_time_seconds_start = start_time_seconds;
    this->saveMilestoneToPowerLoss();
}

void LocalController::stopStepTimeCounter() {
    this->state->step_time_seconds_start = 0;
}

unsigned long LocalController::getStepTimeStart() {
    return this->state->step_time_seconds_start;
}

unsigned long LocalController::getStepElapsedTime() {
    if (this->state->step_time_seconds_start == 0) {
        return 0;
    }
    return this->now() - this->state->step_time_seconds_start;
}

void LocalController::setStepEstimatedTime(unsigned long estimatedTime_seconds) {
    this->state->step_time_seconds_estimated = estimatedTime_seconds;
    this->saveMilestoneToPowerLoss();
}

void LocalController::waitForStepTime() {
    this->waitForTimer(this->state->step_time_seconds_estimated - this->getStepElapsedTime());
}

// Timer Management
void LocalController::waitForTimer(unsigned long duration_seconds) {
    this->state->target_timer_time_seconds = this->now() + duration_seconds;
    this->task->setState(&timerState);
    this->saveMilestoneToPowerLoss();
}

void LocalController::stopTimer() {
    this->state->target_timer_time_seconds = 0;
}

bool LocalController::isTimeFinished() {
    return this->state->target_timer_time_seconds < this->now();
}

// ============================================================================
// POWER RECOVERY AND PERSISTENCE
// ============================================================================

void LocalController::saveMilestoneToPowerLoss() {
    this->powerRecovery.saveState(this->state);
}

void LocalController::deleteMilestoneFromPowerLoss() {
    this->powerRecovery.deleteState();
}

void LocalController::restoreStateFromPowerLoss() 
{

    MachineState storedState;


    if(!this->powerRecovery.loadState(&storedState)){
        ESP_LOGI("LocalController", "No state found");
        return;
    }


    if(storedState.version != this->state->version){
        ESP_LOGI("LocalController", "Invalid state");
        return;
    }


    if(strlen(storedState.file_name) > 0){
        ESP_LOGI("LocalController", "Opening file ");
        openFile(storedState.file_name);

        if(sdCardState.isFileOpen){
            ESP_LOGI("LocalController", "File recovered from power loss");
            ESP_LOGI("LocalController", "Seeking to ");
            ESP_LOGI("LocalController", "%d", storedState.file_position);
            sdCardState.file->seek(storedState.file_position);
        }
    }

    this->setStep(storedState.step);
    if(storedState.total_time_seconds_start) {
        ESP_LOGI("LocalController", "Resuming total time counter ");
        ESP_LOGI("LocalController", "%d", storedState.total_time_seconds_start);
        this->startTotalTimeCounter(storedState.total_time_seconds_start);
    }
    if(storedState.total_time_seconds_estimated) {
        ESP_LOGI("LocalController", "Resuming step time counter ");
        ESP_LOGI("LocalController", "%d", storedState.total_time_seconds_estimated);
        this->setEstimatedTime(storedState.total_time_seconds_estimated);
    }
    if(storedState.step_time_seconds_start) {
        ESP_LOGI("LocalController", "Resuming step time counter ");
        ESP_LOGI("LocalController", "%d", storedState.step_time_seconds_start);
        this->startStepTimeCounter(storedState.step_time_seconds_start);
    }
    if(storedState.step_time_seconds_estimated) {
        ESP_LOGI("LocalController", "Resuming step time counter ");
        ESP_LOGI("LocalController", "%d", storedState.step_time_seconds_estimated);
        this->setStepEstimatedTime(storedState.step_time_seconds_estimated);
    }


    

    switch (storedState.current)
    {
    case StateType::WAIT_TEMPERATURE: {
        ESP_LOGI("LocalController", "Resuming Temperature ");
        ESP_LOGI("LocalController", "%f", storedState.target_temperature_c);
        this->setTargetTemperatureAndWait(storedState.target_temperature_c);
        break;
    }

    case StateType::PREPARING: {
        ESP_LOGI("LocalController", "Resuming preparing ");
        ESP_LOGI("LocalController", "%f", storedState.target_temperature_c);
        ESP_LOGI("LocalController", "%d", storedState.target_timer_time_seconds);

        auto current = this->now();

        this->prepareTemperature(storedState.target_temperature_c, storedState.target_timer_time_seconds - current);

        break;
    }

    case StateType::WAIT_TIMER: {

        ESP_LOGI("LocalController", "Resuming timer ");
        ESP_LOGI("LocalController", "%d", storedState.target_timer_time_seconds);

        auto current = this->now();

        this->waitForTimer(storedState.target_timer_time_seconds - current);
        break;
    }

    case StateType::WAIT_CONFIRM: {
        ESP_LOGI("LocalController", "Resuming confirm ");
        this->waitConfirmation();
        break;
    }

    default:
        ESP_LOGI("LocalController", "Unknown state ");
        ESP_LOGI("LocalController", "%d", storedState.current);
        break;
    }


    // Delete file
    ESP_LOGI("LocalController", "Deleting file ");
    ESP_LOGI("LocalController", "%s", POWER_LOSS_RECOVERY_FILE);
    this->powerRecovery.deleteState();
}

// ============================================================================
// UTILITY AND HELPER METHODS
// ============================================================================

float LocalController::getVolume() {
    return this->settings->getVolumeLiters();
}

float LocalController::getPower() {
    return this->settings->getPowerWatts();
}