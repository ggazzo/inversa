#ifndef POWER_RECOVERY_H
#define POWER_RECOVERY_H

#include "definitions.h"
#include "SD.h"

template<typename T>
class PowerRecovery {
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
    return true;
}
#endif
