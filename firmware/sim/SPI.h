#pragma once
#include "Arduino.h"
class SPIClass {
public:
    bool begin(int /*sck*/ = -1, int /*miso*/ = -1, int /*mosi*/ = -1, int /*cs*/ = -1) { return true; }
};
extern SPIClass SPI;
