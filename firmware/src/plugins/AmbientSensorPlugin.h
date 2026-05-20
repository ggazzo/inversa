#pragma once

// AmbientSensorPlugin — placeholder. No hardware ambient sensor is
// wired today; users enter ambient manually through the thermal
// settings sheet. This stub exists so the wire shape between gState
// (ambientSource/ambientSensorC/ambientSensorOk/ambientSensorLastMs),
// getEffectiveAmbient(), the BLE protocol (evt:status fields ambEff/
// ambSrc/ambSensorOk), and the UI badge is locked now. When the
// hardware lands, fill in loop() to read the sensor and write the
// gState fields; no consumer of getEffectiveAmbient() needs to move.
//
// Contract for the future implementation:
//   - Sample at ≤ 1 Hz (ambient changes slowly).
//   - On a good reading: write gState.ambientSensorC, set
//     gState.ambientSensorOk = true, gState.ambientSensorLastMs = millis().
//   - On bad reading (timeout, CRC, NaN, etc.): set
//     gState.ambientSensorOk = false. Leave the last good
//     ambientSensorC untouched — getEffectiveAmbient() will already
//     fall back to manual via the freshness check.
//   - Do NOT write gState.ambientTemp (user-owned manual value).
//   - Do NOT decide which source feeds consumers; that is what
//     gState.ambientSource and getEffectiveAmbient() are for.

#include <Arduino.h>
#include "../core/Plugin.h"
#include "../models/MachineState.h"

class AmbientSensorPlugin : public Plugin {
public:
    const char* getName() const override { return "AmbientSensor"; }

    bool setup() override {
        // No hardware yet. Mark sensor as not-ok so getEffectiveAmbient()
        // falls back to manual regardless of ambientSource setting.
        gState.ambientSensorOk     = false;
        gState.ambientSensorLastMs = 0;
        return true;
    }

    void loop() override {
        // Intentionally empty. Future impl reads sensor here.
    }
};
