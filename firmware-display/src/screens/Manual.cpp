#include "Manual.h"
#include "Theme.h"
#include "../state/AppState.h"
#include "../ble/BleClient.h"
#ifdef USE_SQUARELINE_UI
#include "../ui/sq/sq_ui.h"
#endif
#include <cstdio>

namespace inversa { namespace display {

namespace {

lv_obj_t* s_scr = nullptr;
lv_obj_t* s_lblTemp;
lv_obj_t* s_slider;
lv_obj_t* s_lblTarget;
lv_obj_t* s_switch;

// Slider released → push set-temp. We only send on release (not every
// step) so dragging doesn't spam the controller.
void on_slider_release(lv_event_t* e) {
    lv_obj_t* sl = lv_event_get_target_obj(e);
    int v = lv_slider_get_value(sl);
    ble_send_set_temp((float)v);
}

void on_switch(lv_event_t* e) {
    lv_obj_t* sw = lv_event_get_target_obj(e);
    bool on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    ble_send_heater(on);
}

}  // namespace

lv_obj_t* manual_screen_create() {
#ifdef USE_SQUARELINE_UI
    s_scr        = ui_Manual;
    s_lblTemp    = ui_Manual_lblTemp;
    s_lblTarget  = ui_Manual_lblTarget;
    s_slider     = ui_Manual_slider;
    s_switch     = ui_Manual_switch;
    lv_obj_add_event_cb(s_slider, on_slider_release, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(s_switch, on_switch, LV_EVENT_VALUE_CHANGED, nullptr);
    return s_scr;
#else
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, theme_bg(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 10, 0);
    lv_obj_set_flex_flow(s_scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_scr, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* title = lv_label_create(s_scr);
    lv_label_set_text(title, "Manual");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, theme_muted(), 0);

    s_lblTemp = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblTemp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lblTemp, theme_primary(), 0);
    lv_label_set_text(s_lblTemp, "--");

    s_lblTarget = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblTarget, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lblTarget, theme_accent(), 0);
    lv_label_set_text(s_lblTarget, "→ 65°");

    s_slider = lv_slider_create(s_scr);
    lv_obj_set_size(s_slider, 150, 22);
    lv_slider_set_range(s_slider, 22, 100);
    lv_slider_set_value(s_slider, 65, LV_ANIM_OFF);
    lv_obj_add_event_cb(s_slider, on_slider_release, LV_EVENT_RELEASED, nullptr);
    lv_obj_set_style_bg_color(s_slider, theme_surface(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_slider, theme_accent(), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_slider, theme_primary(), LV_PART_KNOB);

    lv_obj_t* row = lv_obj_create(s_scr);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 8, 0);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, "Resistencia");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, theme_muted(), 0);

    s_switch = lv_switch_create(row);
    lv_obj_set_size(s_switch, 50, 26);
    lv_obj_add_event_cb(s_switch, on_switch, LV_EVENT_VALUE_CHANGED, nullptr);

    return s_scr;
#endif  // USE_SQUARELINE_UI
}

void manual_screen_update() {
    if (!s_scr) return;
    auto& s = app_state();
    char buf[24];

    snprintf(buf, sizeof(buf), "%.1f°", s.currentTemp.load());
    lv_label_set_text(s_lblTemp, buf);

    float tg = s.targetTemp.load();
    snprintf(buf, sizeof(buf), "→ %.0f°", tg);
    lv_label_set_text(s_lblTarget, buf);

    // Only sync slider to state if the user isn't currently dragging.
    if (!lv_obj_has_state(s_slider, LV_STATE_PRESSED)) {
        lv_slider_set_value(s_slider, (int)tg, LV_ANIM_OFF);
    }

    bool heaterOn = s.heaterOn.load();
    bool checked = lv_obj_has_state(s_switch, LV_STATE_CHECKED);
    if (heaterOn != checked) {
        if (heaterOn) lv_obj_add_state(s_switch, LV_STATE_CHECKED);
        else          lv_obj_clear_state(s_switch, LV_STATE_CHECKED);
    }
}

}}  // namespace inversa::display
