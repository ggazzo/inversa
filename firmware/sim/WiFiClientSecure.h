#pragma once
#include "Arduino.h"
class WiFiClient {
public:
    int    connect(const char*, int) { return 0; }
    bool   connected()               { return false; }
    int    available()               { return 0; }
    int    read()                    { return -1; }
    size_t readBytes(uint8_t*, size_t) { return 0; }
};
class WiFiClientSecure : public WiFiClient {
public:
    void setInsecure() {}
};
