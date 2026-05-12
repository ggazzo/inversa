#pragma once
#include <cstdint>
#include <atomic>

// Virtual clock for the brewing simulator. Replaces millis()/micros() with
// a counter the sim driver advances explicitly. Decoupling firmware time
// from wall time lets us run a 4-hour brew in minutes (`--scale 60`) or in
// CPU-bound mode for CI (`--scale 0`).

class SimClock {
public:
    static uint32_t nowMs()  { return _ms.load(); }
    static uint32_t nowUs()  { return _ms.load() * 1000u; }

    static void advanceMs(uint32_t deltaMs) { _ms.fetch_add(deltaMs); }
    static void reset()                     { _ms.store(0); }

    // Signaled by main loop on SIGINT, RecipeCompleted, or max-sim-time.
    static bool shouldExit()           { return _exit.load(); }
    static void requestExit()          { _exit.store(true); }

private:
    inline static std::atomic<uint32_t> _ms{0};
    inline static std::atomic<bool>     _exit{false};
};
