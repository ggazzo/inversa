#pragma once
#include <string>
#include <cstdlib>
#include <cstring>
#include <cstdio>

struct SimArgs {
    std::string recipePath;
    float scale       = 60.0f;   // 1 real sec = `scale` sim sec; 0 = unthrottled
    uint32_t maxSimSec = 6 * 3600;  // hard ceiling — 6 hours simulated
    bool exitOnComplete = true;

    // ThermalSim params (mirror P13 NVS keys)
    float volumeLiters = 25.0f;
    float heaterPowerW = 3000.0f;
    float ambientC     = 22.0f;
    float diameterM    = 0.35f;
    float lossCoeff    = 10.0f;
};

inline void printSimUsage() {
    std::fprintf(stderr,
        "Inversa brewing simulator\n"
        "Usage: inversa_sim [opts]\n"
        "  --recipe <path>        Auto-load + start this recipe at boot\n"
        "  --scale <N>            Time scale (default 60; 0 = no throttle)\n"
        "  --max-sim-time <sec>   Abort after N simulated seconds (default 21600)\n"
        "  --no-exit-on-complete  Stay alive after RecipeCompleted\n"
        "  --volume <L>           Water volume liters (default 25)\n"
        "  --power <W>            Heater power Watts (default 3000)\n"
        "  --ambient <C>          Ambient temp °C (default 22)\n"
        "  --diameter <m>         Vessel diameter meters (default 0.35)\n"
        "  --loss <h>             Heat loss coeff W/m²K (default 10)\n");
}

inline bool parseSimArgs(int argc, char** argv, SimArgs& out) {
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        auto next = [&](const char* opt) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "[sim] missing value for %s\n", opt);
                return nullptr;
            }
            return argv[++i];
        };
        if      (std::strcmp(a, "--recipe") == 0)      { auto v = next(a); if (!v) return false; out.recipePath = v; }
        else if (std::strcmp(a, "--scale") == 0)       { auto v = next(a); if (!v) return false; out.scale = (float)std::atof(v); }
        else if (std::strcmp(a, "--max-sim-time") == 0){ auto v = next(a); if (!v) return false; out.maxSimSec = (uint32_t)std::atoi(v); }
        else if (std::strcmp(a, "--no-exit-on-complete") == 0) out.exitOnComplete = false;
        else if (std::strcmp(a, "--volume") == 0)      { auto v = next(a); if (!v) return false; out.volumeLiters = (float)std::atof(v); }
        else if (std::strcmp(a, "--power") == 0)       { auto v = next(a); if (!v) return false; out.heaterPowerW = (float)std::atof(v); }
        else if (std::strcmp(a, "--ambient") == 0)     { auto v = next(a); if (!v) return false; out.ambientC     = (float)std::atof(v); }
        else if (std::strcmp(a, "--diameter") == 0)    { auto v = next(a); if (!v) return false; out.diameterM    = (float)std::atof(v); }
        else if (std::strcmp(a, "--loss") == 0)        { auto v = next(a); if (!v) return false; out.lossCoeff    = (float)std::atof(v); }
        else if (std::strcmp(a, "-h") == 0 || std::strcmp(a, "--help") == 0) {
            printSimUsage(); std::exit(0);
        }
        else {
            std::fprintf(stderr, "[sim] unknown arg: %s\n", a);
            printSimUsage();
            return false;
        }
    }
    return true;
}
