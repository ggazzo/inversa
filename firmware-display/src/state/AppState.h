#pragma once

// AppState — mirror of the brewing controller's state, fed by BleClient
// from the Nordic UART notifies it subscribes to. UI screens (handwritten
// or SquareLine-generated) read this struct; LvglBridge.cpp pushes
// changes into the LVGL widget tree on a coalesced redraw tick.
//
// Keep field names short — they map directly to the keys in the
// controller's `evt:status` JSON (firmware/src/protocol/protocol.h).

#include <cstdint>
#include <atomic>

namespace inversa { namespace display {

struct AppState {
    // ── Connection ────────────────────────────────────────────
    std::atomic<bool>   bleConnected{false};
    char                deviceName[24] = "Inversa";

    // ── Temperature loop ──────────────────────────────────────
    std::atomic<float>  currentTemp{0.0f};      // gState.currentTemp
    std::atomic<float>  targetTemp{0.0f};       // gState.targetTemp
    std::atomic<float>  pidOutput{0.0f};
    std::atomic<bool>   heaterOn{false};
    std::atomic<bool>   pumpOn{false};
    std::atomic<int>    ssrLevel{0};
    std::atomic<bool>   tempSensorOk{false};

    // ── Mode / recipe ─────────────────────────────────────────
    char                mode[12] = "idle";       // idle / manual / recipe / autotune
    char                recipeState[20] = "";    // running / paused / waiting_temp / ...
    char                recipeName[32] = "";
    char                recipeStepName[32] = "";
    std::atomic<int>    recipeStep{0};
    std::atomic<int>    recipeTotal{0};

    // ── Boil ──────────────────────────────────────────────────
    std::atomic<bool>   boilActive{false};
    std::atomic<int>    boilRemaining{0};        // seconds
    std::atomic<int>    boilTotal{0};

    // ── Scheduler ─────────────────────────────────────────────
    std::atomic<bool>   schedActive{false};
    std::atomic<int>    schedTargetHour{0};
    std::atomic<int>    schedTargetMin{0};
    std::atomic<float>  schedTargetTemp{0.0f};
    std::atomic<float>  schedVolume{0.0f};
    char                schedStatus[32] = "";

    // ── Watchdog ──────────────────────────────────────────────
    std::atomic<bool>   wdSupported{false};
    std::atomic<bool>   wdArmed{false};
    std::atomic<bool>   wdTripped{false};
    char                wdCause[20] = "";
    std::atomic<float>  wdHardStopC{105.0f};

    // ── RTC ───────────────────────────────────────────────────
    std::atomic<bool>   rtcAvailable{false};
    std::atomic<bool>   rtcNtpSynced{false};
    std::atomic<uint32_t> rtcUnix{0};

    // ── Hop alerts (queue mirror) ────────────────────────────
    // Up to 8 pending notifications from evt:boil:addition. The bridge
    // pops one when the user taps Confirmar; the firmware queue (when
    // it lands — see firmware/docs/hop-queue-plan.md) will replace this
    // local buffer.
    static constexpr size_t MAX_HOPS = 8;
    struct HopAlert {
        char     name[24];
        int      minMark;
        uint32_t firedAtMs;
    };
    HopAlert hops[MAX_HOPS] = {};
    std::atomic<size_t> hopHead{0};   // index of oldest unacked
    std::atomic<size_t> hopTail{0};   // next write slot

    // Update generation — bumped by BleClient when any field changes.
    // The bridge wakes once per tick and re-pushes everything if this
    // moved.
    std::atomic<uint32_t> generation{0};
};

AppState& app_state();

}}  // namespace inversa::display
