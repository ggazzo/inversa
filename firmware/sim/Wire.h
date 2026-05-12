#pragma once
#include "Arduino.h"
class TwoWire {
public:
    bool begin(int /*sda*/, int /*scl*/) { return true; }
};
extern TwoWire Wire;
