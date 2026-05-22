// LvglBridge — picks the active screen by AppState mode/state and pushes
// values into widget trees every tick when `generation` advances.
//
// Dispatch rules (top-most wins):
//   1. BLE not connected            → Scanning
//   2. wdTripped                    → Watchdog overlay over current screen
//   3. schedActive && mode==idle    → Scheduled
//   4. mode == recipe               → Recipe (boil sub-state handled inside)
//   5. mode == manual               → Manual
//   6. default                      → Home
//   Hop alerts: shown as a modal overlay on top of any non-Scanning view
//   when there's at least one pending alert in the AppState ring.

#include "LvglBridge.h"
#include <Arduino.h>
#include "../state/AppState.h"
#include "../screens/Scanning.h"
#include "../screens/Home.h"
#include "../screens/Recipe.h"
#include "../screens/Manual.h"
#include "../screens/Scheduled.h"
#include "../screens/Watchdog.h"
#include "../screens/HopAlert.h"
#ifdef USE_SQUARELINE_UI
#include "../ui/sq/sq_ui.h"
#endif
#include <cstring>

namespace inversa { namespace display {

namespace {

enum class View { None, Scanning, Home, Recipe, Manual, Scheduled };

// s_view starts as None so bridge_init's first switch_to(Scanning) is
// guaranteed to fall through the early-out (s_view == v) and actually
// call lv_screen_load. Otherwise LVGL would keep its auto-allocated
// default screen as active and render its empty theme bg forever.
View      s_view = View::None;
uint32_t  s_lastGen = 0;

lv_obj_t* s_scrScanning = nullptr;
lv_obj_t* s_scrHome     = nullptr;
lv_obj_t* s_scrRecipe   = nullptr;
lv_obj_t* s_scrManual   = nullptr;
lv_obj_t* s_scrSched    = nullptr;

// Tracks the last hop tail we showed an overlay for, so we re-trigger
// the modal whenever a new alert lands.
size_t    s_lastHopTail = 0;

lv_obj_t* screen_for(View v) {
    switch (v) {
        case View::None:      return nullptr;
        case View::Scanning:
            return s_scrScanning ? s_scrScanning
                                 : (s_scrScanning = scanning_screen_create());
        case View::Home:
            return s_scrHome     ? s_scrHome
                                 : (s_scrHome     = home_screen_create());
        case View::Recipe:
            return s_scrRecipe   ? s_scrRecipe
                                 : (s_scrRecipe   = recipe_screen_create());
        case View::Manual:
            return s_scrManual   ? s_scrManual
                                 : (s_scrManual   = manual_screen_create());
        case View::Scheduled:
            return s_scrSched    ? s_scrSched
                                 : (s_scrSched    = scheduled_screen_create());
    }
    return nullptr;
}

View pick_view(const AppState& s) {
    if (!s.bleConnected.load())   return View::Scanning;
    if (s.schedActive.load() && strcmp(s.mode, "idle") == 0)
                                   return View::Scheduled;
    if (strcmp(s.mode, "recipe") == 0)   return View::Recipe;
    if (strcmp(s.mode, "manual") == 0)   return View::Manual;
    return View::Home;
}

void update_active() {
    switch (s_view) {
        case View::None:                                break;
        case View::Scanning:                       break;  // static
        case View::Home:      home_screen_update();     break;
        case View::Recipe:    recipe_screen_update();   break;
        case View::Manual:    manual_screen_update();   break;
        case View::Scheduled: scheduled_screen_update();break;
    }
}

void switch_to(View v) {
    if (s_view == v) return;
    lv_obj_t* scr = screen_for(v);
    lv_screen_load(scr);  // v9 canonical name; lv_scr_load is the legacy alias
    Serial.printf("[BRIDGE] loaded screen %p\n", (void*)scr);
    s_view = v;
}

}  // namespace

void bridge_init() {
#ifdef USE_SQUARELINE_UI
    // SquareLine's ui_init() allocates every screen as static globals.
    // Must run before any screen_create() reaches for those globals.
    ui_init();
#endif
    switch_to(View::Scanning);
}

void bridge_tick() {
    auto& s = app_state();

    View v = pick_view(s);
    switch_to(v);

    uint32_t gen = s.generation.load();
    if (gen != s_lastGen) {
        s_lastGen = gen;
        update_active();

        // Watchdog overlay — top priority. Show whenever tripped, hide
        // automatically when controller reports it cleared.
        if (s.wdTripped.load()) {
            watchdog_overlay_show();
            watchdog_overlay_update();
        } else if (watchdog_overlay_visible()) {
            watchdog_overlay_hide();
        }

        // Hop alert overlay — only when a new addition arrives. Cleared
        // by the user via Confirmar (handled inside HopAlert).
        size_t head = s.hopHead.load();
        size_t tail = s.hopTail.load();
        if (head != tail && tail != s_lastHopTail) {
            const auto& h = s.hops[head];
            Serial.printf("[BRIDGE] hop_alert_show head=%u tail=%u name=\"%s\"\n",
                          (unsigned)head, (unsigned)tail, h.name);
            hop_alert_show(h.name, h.minMark);
            s_lastHopTail = tail;
        } else if (head == tail && hop_alert_visible()) {
            hop_alert_hide();
        }
    }
}

}}  // namespace inversa::display
