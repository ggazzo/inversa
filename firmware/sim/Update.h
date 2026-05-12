#pragma once
#include "Arduino.h"
class UpdateClass {
public:
    bool begin(size_t)              { return false; }
    size_t write(uint8_t*, size_t)  { return 0; }
    bool end(bool /*evict*/ = false){ return false; }
    void abort()                    {}
};
extern UpdateClass Update;
