#include "Watchdog.h"
#include "Theme.h"
#include "../state/AppState.h"
#include "../ble/BleClient.h"
#ifdef USE_SQUARELINE_UI
#include "../ui/sq/sq_ui.h"
#endif
#include <Arduino.h>
#include <cstdio>

namespace inversa { namespace display {

namespace {

lv_obj_t* s_overlay = nullptr;
lv_obj_t* s_lblCause;
lv_obj_t* s_lblHard;

void on_reset(lv_event_t*) {
    Serial.println("[Watchdog] RESET button tapped → req:watchdog:reset");
    ble_send_watchdog_reset();
}

void ensure_created() {
    if (s_overlay) return;
#ifdef USE_SQUARELINE_UI
    s_overlay  = ui_Watchdog;
    s_lblCause = ui_Watchdog_lblCause;
    s_lblHard  = ui_Watchdog_lblHard;
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ui_Watchdog_btnReset, on_reset, LV_EVENT_CLICKED, nullptr);
    return;
#else
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_overlay, 172, 320);
    lv_obj_align(s_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, theme_danger(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_overlay, 10, 0);
    lv_obj_set_flex_flow(s_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_overlay, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_overlay, 8, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* icon = lv_label_create(s_overlay);
    lv_label_set_text(icon, LV_SYMBOL_WARNING);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, theme_primary(), 0);

    lv_obj_t* title = lv_label_create(s_overlay);
    lv_label_set_text(title, "WATCHDOG");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, theme_primary(), 0);

    s_lblCause = lv_label_create(s_overlay);
    lv_label_set_long_mode(s_lblCause, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lblCause, 150);
    lv_obj_set_style_text_align(s_lblCause, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_lblCause, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblCause, theme_primary(), 0);
    lv_label_set_text(s_lblCause, "");

    s_lblHard = lv_label_create(s_overlay);
    lv_obj_set_style_text_font(s_lblHard, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblHard, theme_primary(), 0);
    lv_label_set_text(s_lblHard, "");

    lv_obj_t* btn = lv_button_create(s_overlay);
    lv_obj_set_size(btn, 130, 44);
    lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_add_event_cb(btn, on_reset, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(btn);
    lv_label_set_text(bl, "RESET");
    lv_obj_set_style_text_color(bl, theme_danger(), 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_center(bl);
#endif  // USE_SQUARELINE_UI
}

}  // namespace

void watchdog_overlay_show() {
    ensure_created();
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
}

void watchdog_overlay_hide() {
    if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

bool watchdog_overlay_visible() {
    return s_overlay && !lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void watchdog_overlay_update() {
    if (!s_overlay) return;
    auto& s = app_state();
    char buf[32];
    lv_label_set_text(s_lblCause,
        s.wdCause[0] ? s.wdCause : "Causa desconhecida");
    snprintf(buf, sizeof(buf), "hard-stop %.1f°", s.wdHardStopC.load());
    lv_label_set_text(s_lblHard, buf);
}

}}  // namespace inversa::display
