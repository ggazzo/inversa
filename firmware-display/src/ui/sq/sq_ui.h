#pragma once

// SquareLine UI contract for Inversa Display.
//
// This header declares the screens and widget pointers that SquareLine
// Studio must export when USE_SQUARELINE_UI is defined. The handwritten
// screens under src/screens/*.cpp will, when that flag is on, skip their
// own widget construction and bind these externs instead.
//
// Workflow:
//   1. In SquareLine Studio, name each widget exactly as listed below
//      (case-sensitive). The Studio's "Object Name" field maps to the
//      `ui_<Screen>_<widget>` global it generates.
//   2. Export with target "Arduino with TFT_eSPI" or any LVGL v9 target —
//      the generated C calls only public LVGL APIs.
//   3. Drop the export into `src/ui/sq/` (replacing this stub). The export
//      should provide one `ui.h` / `ui.c` pair plus per-screen files.
//   4. Define `USE_SQUARELINE_UI` in platformio.ini and rebuild.
//
// Required widget names per screen — handwritten fallbacks crash early
// (linker error) if any of these is missing on the SquareLine side.

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// One-time UI initializer emitted by SquareLine. Allocates all screens.
void ui_init(void);

// ── Screen: Home ─────────────────────────────────────────────────────
extern lv_obj_t* ui_Home;             // root lv_obj_t*
extern lv_obj_t* ui_Home_lblTemp;     // big current-temp label
extern lv_obj_t* ui_Home_lblTarget;   // "alvo XX.X°" label
extern lv_obj_t* ui_Home_lblMode;     // mode label (Idle / Receita / …)
extern lv_obj_t* ui_Home_lblClock;    // top-bar clock HH:MM
extern lv_obj_t* ui_Home_arc;         // ramp progress arc
extern lv_obj_t* ui_Home_dotHeater;   // heater status pill
extern lv_obj_t* ui_Home_dotPump;     // pump status pill

// ── Screen: Recipe ───────────────────────────────────────────────────
extern lv_obj_t* ui_Recipe;
extern lv_obj_t* ui_Recipe_lblName;
extern lv_obj_t* ui_Recipe_lblStep;
extern lv_obj_t* ui_Recipe_lblStepIdx;
extern lv_obj_t* ui_Recipe_lblTemp;
extern lv_obj_t* ui_Recipe_lblTarget;
extern lv_obj_t* ui_Recipe_bar;       // step progress bar
extern lv_obj_t* ui_Recipe_lblBoil;   // boil countdown mm:ss
extern lv_obj_t* ui_Recipe_btnRow;    // container holding the 3 buttons
extern lv_obj_t* ui_Recipe_btnConfirm;
extern lv_obj_t* ui_Recipe_btnPause;
extern lv_obj_t* ui_Recipe_lblBtnPause;  // label inside Pause btn (toggles Play/Pause)
extern lv_obj_t* ui_Recipe_btnStop;

// ── Screen: Manual ───────────────────────────────────────────────────
extern lv_obj_t* ui_Manual;
extern lv_obj_t* ui_Manual_lblTemp;
extern lv_obj_t* ui_Manual_lblTarget;
extern lv_obj_t* ui_Manual_slider;    // range 22..100
extern lv_obj_t* ui_Manual_switch;    // heater on/off

// ── Screen: Scheduled ────────────────────────────────────────────────
extern lv_obj_t* ui_Scheduled;
extern lv_obj_t* ui_Scheduled_lblTarget;     // "07:30"
extern lv_obj_t* ui_Scheduled_lblCountdown;  // "em 2h05m"
extern lv_obj_t* ui_Scheduled_lblTemp;       // "alvo 65°"
extern lv_obj_t* ui_Scheduled_lblVol;        // "vol 20L"
extern lv_obj_t* ui_Scheduled_lblStatus;     // free-text from controller
extern lv_obj_t* ui_Scheduled_btnStop;

// ── Overlay: HopAlert ────────────────────────────────────────────────
// Built as a screen in SquareLine (set Background opa so it reads as a
// modal over the active screen). Bridge moves it to lv_layer_top() on
// first show.
extern lv_obj_t* ui_HopAlert;
extern lv_obj_t* ui_HopAlert_lblName;
extern lv_obj_t* ui_HopAlert_lblMin;
extern lv_obj_t* ui_HopAlert_btnConfirm;

// ── Overlay: Watchdog ────────────────────────────────────────────────
extern lv_obj_t* ui_Watchdog;
extern lv_obj_t* ui_Watchdog_lblCause;
extern lv_obj_t* ui_Watchdog_lblHard;     // "hard-stop 105°"
extern lv_obj_t* ui_Watchdog_btnReset;

// ── Screen: Scanning (optional) ──────────────────────────────────────
// SquareLine override is optional here; if you don't export this screen,
// the handwritten version stays in use (see screens/Scanning.cpp).
extern lv_obj_t* ui_Scanning;  // weak — may be nullptr

#ifdef __cplusplus
}  // extern "C"
#endif
