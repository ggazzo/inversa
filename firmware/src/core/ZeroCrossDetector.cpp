#include "ZeroCrossDetector.h"

#ifndef NATIVE_BUILD

ZeroCrossDetector& ZeroCrossDetector::instance() {
    static ZeroCrossDetector inst;
    return inst;
}

void ZeroCrossDetector::begin(uint8_t pin, uint16_t mainsFreqHz, int edgeMode) {
    _pin             = pin;
    _freq            = mainsFreqHz ? mainsFreqHz : 60;
    _halfPeriodUs    = 1000000UL / (2UL * _freq);   // 8333 @60Hz, 10000 @50Hz
    _debounceUs      = (_halfPeriodUs * 7) / 10;     // 70% of half period
    _faultTimeoutUs  = _halfPeriodUs * 3;            // 3 missed half-cycles
    _halfCycleCount  = 0;
    _lastIsrUs       = micros();
    pinMode(pin, INPUT);
    attachInterruptArg(
        digitalPinToInterrupt(pin),
        &ZeroCrossDetector::isrThunk,
        this,
        edgeMode);
}

bool ZeroCrossDetector::isFaulted() const {
    uint32_t now  = micros();
    uint32_t last = _lastIsrUs;
    return (now - last) > _faultTimeoutUs;
}

void IRAM_ATTR ZeroCrossDetector::isrThunk(void* arg) {
    ZeroCrossDetector* self = static_cast<ZeroCrossDetector*>(arg);
    uint32_t now = micros();
    if ((now - self->_lastIsrUs) < self->_debounceUs) return;
    self->_lastIsrUs      = now;
    self->_halfCycleCount = self->_halfCycleCount + 1;
}

#endif  // NATIVE_BUILD
