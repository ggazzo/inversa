#pragma once

// IExternalCut — abstract hook for a secondary, hardware-backed kill path
// for the SSR (001-thermal-watchdog, RF-10).
//
// The Thermal Watchdog always cuts power by writing LOW to PIN_HEATER_SSR
// directly. If a board ships with a redundant cut path (a second GPIO in
// series with the SSR control, an external MCU latch, a comparator, etc.),
// it implements this interface and registers itself via
// `ThermalWatchdog::setExternalCut(IExternalCut*)`. When the watchdog
// trips, both paths fire — there is no fallback dependency.
//
// No concrete implementation ships with this feature; this is an
// extension point. See _reversa_forward/001-thermal-watchdog/roadmap.md
// decision D-09 for context.

#include "../models/MachineState.h"  // WatchdogCause

class IExternalCut {
public:
    virtual ~IExternalCut() = default;

    // Called from the watchdog the instant a trip is detected. Must be
    // fast and non-blocking — implementations are expected to write a
    // GPIO or set a latch flag, not initiate BLE/SD/NVS work.
    virtual void trip(WatchdogCause cause) = 0;

    // Optional. Called when the watchdog latch is cleared (manual or
    // auto). Default no-op so implementations only override when needed.
    virtual void clear() {}
};
