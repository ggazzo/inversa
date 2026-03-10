#pragma once
#include <Arduino.h>
#include "EventBus.h"

class Plugin {
public:
    virtual ~Plugin() = default;

    // Lifecycle
    virtual const char* getName() const = 0;
    virtual bool setup() = 0;           // Called once during init. Return false to disable.
    virtual void loop() = 0;            // Called every main loop iteration
    
    // Enable/disable
    bool isEnabled() const { return _enabled; }
    void setEnabled(bool enabled) { _enabled = enabled; }

protected:
    EventBus& bus() { return EventBus::instance(); }
    bool _enabled = true;
};
