// LvglBridge — owns the current LVGL screen + writes AppState values
// into its labels. Coarse-grained: rebuilt on connection-state edges,
// individual labels updated every tick when `generation` moved.

#include "LvglBridge.h"
#include "../state/AppState.h"
#include "../screens/Scanning.h"
#include <cstdio>

namespace inversa { namespace display {

namespace {

enum class View { Scanning, Home };

View       s_view = View::Scanning;
uint32_t   s_lastGen = 0;

lv_obj_t* s_home_scr  = nullptr;
lv_obj_t* s_lbl_temp  = nullptr;
lv_obj_t* s_lbl_mode  = nullptr;
lv_obj_t* s_lbl_target = nullptr;
lv_obj_t* s_bar_temp  = nullptr;

lv_obj_t* home_screen_create() {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    s_lbl_temp = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_temp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lbl_temp, lv_color_white(), 0);
    lv_obj_align(s_lbl_temp, LV_ALIGN_TOP_MID, 0, 30);
    lv_label_set_text(s_lbl_temp, "--.- °C");

    s_lbl_target = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_target, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lbl_target,
        lv_color_make(0xA0, 0xA0, 0xA0), 0);
    lv_obj_align(s_lbl_target, LV_ALIGN_TOP_MID, 0, 100);
    lv_label_set_text(s_lbl_target, "→ --.-");

    s_bar_temp = lv_bar_create(scr);
    lv_obj_set_size(s_bar_temp, 140, 10);
    lv_obj_align(s_bar_temp, LV_ALIGN_TOP_MID, 0, 150);
    lv_bar_set_range(s_bar_temp, 0, 100);
    lv_bar_set_value(s_bar_temp, 0, LV_ANIM_OFF);

    s_lbl_mode = lv_label_create(scr);
    lv_obj_set_style_text_font(s_lbl_mode, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lbl_mode, lv_color_white(), 0);
    lv_obj_align(s_lbl_mode, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_label_set_text(s_lbl_mode, "Idle");

    return scr;
}

void switch_to(View v) {
    if (s_view == v) return;
    lv_obj_t* scr = nullptr;
    if (v == View::Scanning) scr = scanning_screen_create();
    else                     scr = s_home_scr ? s_home_scr : (s_home_scr = home_screen_create());
    lv_scr_load(scr);
    s_view = v;
}

void apply_state() {
    auto& s = app_state();
    char buf[32];

    snprintf(buf, sizeof(buf), "%.1f °C", s.currentTemp.load());
    lv_label_set_text(s_lbl_temp, buf);

    snprintf(buf, sizeof(buf), "→ %.1f °C", s.targetTemp.load());
    lv_label_set_text(s_lbl_target, buf);

    float cur = s.currentTemp.load();
    float tgt = s.targetTemp.load();
    if (tgt > 22.0f) {
        int pct = int(((cur - 22.0f) / (tgt - 22.0f)) * 100.0f);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        lv_bar_set_value(s_bar_temp, pct, LV_ANIM_OFF);
    }

    lv_label_set_text(s_lbl_mode, s.mode);
}

}  // namespace

void bridge_init() {
    switch_to(View::Scanning);
}

void bridge_tick() {
    auto& s = app_state();
    bool connected = s.bleConnected.load();
    switch_to(connected ? View::Home : View::Scanning);

    uint32_t gen = s.generation.load();
    if (s_view == View::Home && gen != s_lastGen) {
        s_lastGen = gen;
        apply_state();
    }
}

}}  // namespace inversa::display
