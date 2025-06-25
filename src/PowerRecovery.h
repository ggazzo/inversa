#ifndef POWER_RECOVERY_H
#define POWER_RECOVERY_H

#include "definitions.h"
#include "SD.h"
#include "state.h"
#include "media.h"

template<typename T>
class IPowerRecovery {
    public:
        virtual void saveState(T *state) = 0;
        virtual void deleteState() = 0;
        virtual bool loadState(T *state) = 0;
};

template<typename T>
class PowerRecovery : public IPowerRecovery<T> {
    public:
        void saveState(T *state);
        void deleteState();
        bool loadState(T *state);
    private:
        xTimerHandle timerHandle;
        T state;
        void saveStateTask();
};


template<typename T>
void PowerRecovery<T>::saveState(T *state) {
    this->state = *state;
    timerHandle = xTimerCreate(
        "saveStateTimer",
        pdMS_TO_TICKS(1000),
        pdFALSE,
        this,   
        [](TimerHandle_t xTimer) {
            PowerRecovery<T>* recovery = static_cast<PowerRecovery<T>*>(pvTimerGetTimerID(xTimer));
            recovery->saveStateTask();
        }
    );


}

template<typename T>
void PowerRecovery<T>::saveStateTask() {
     File file = SD.open(POWER_LOSS_RECOVERY_FILE, FILE_WRITE);
    if (!file) {
        return;
    }
    file.write((uint8_t*)&this->state, sizeof(T));
    file.close();
}

template<typename T>
void PowerRecovery<T>::deleteState() {
    SD.remove(POWER_LOSS_RECOVERY_FILE);
}

template<typename T>
bool PowerRecovery<T>::loadState(T *state) {
    File file = SD.open(POWER_LOSS_RECOVERY_FILE, FILE_READ);
    if (!file) {
        return false;
    }

    file.read((uint8_t*)state, sizeof(T));
    file.close();



    if(strlen(state->file_name) > 0){
        ESP_LOGI("LocalController", "Opening file ");
        openFile(state->file_name);

        if(state->version != CURRENT_VERSION){
            ESP_LOGI("LocalController", "Invalid state");
            return false;
        }

        if(sdCardState.isFileOpen){
            ESP_LOGI("LocalController", "File recovered from power loss");
            ESP_LOGI("LocalController", "Seeking to ");
            ESP_LOGI("LocalController", "%d", state->file_position);
            sdCardState.file->seek(state->file_position);
        }
    }

    return true;
}
#endif
