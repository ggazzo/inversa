#pragma once

#include <stdint.h>

// Contract for any heater driver implementation.
// Implementations live as Plugin subclasses (e.g. HeaterPlugin for soft-PWM SSR,
// HeaterPluginZC for zero-cross burst-fire). PIDPlugin and the thermal watchdog
// stay driver-agnostic: they publish events and rely on this contract.
class IHeaterDriver {
public:
    virtual ~IHeaterDriver() = default;

    // Master switch. disable() must immediately force the SSR line LOW.
    virtual void enable() = 0;
    virtual void disable() = 0;

    // Set requested duty (0-255). Driver decides how to translate to AC output.
    virtual void setDuty(uint8_t duty) = 0;

    virtual bool isActive() const = 0;
    virtual uint8_t getDutyCycle() const = 0;

    // True when the driver detected a hardware fault that prevents safe operation
    // (e.g. zero-cross signal lost). Default: never faulted.
    virtual bool isFaulted() const { return false; }

    // Burst-fire window in half-cycles. No-op on drivers that don't use a
    // mains-synced window (e.g. soft PWM). Driver enforces a sane minimum.
    virtual void setBurstWindow(uint8_t /*halfCycles*/) {}
    virtual uint8_t getBurstWindow() const { return 0; }
};
