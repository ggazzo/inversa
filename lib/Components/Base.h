#ifndef BASE_H
#define BASE_H

#include <functional>
#include <freertos/FreeRTOS.h>

using on_change_callback_t = std::function<void()>;

class Base {
    public:
        virtual void setup() = 0;
        virtual void loop() = 0;
        on_change_callback_t on_change;
};

#endif
