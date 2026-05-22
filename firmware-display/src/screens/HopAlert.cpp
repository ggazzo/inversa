#include "HopAlert.h"
#include "Theme.h"
#include "../state/AppState.h"
#include "../ble/BleClient.h"
#ifdef USE_SQUARELINE_UI
#include "../ui/sq/sq_ui.h"
#endif
#include <cstdio>

namespace inversa { namespace display {

namespace {

lv_obj_t* s_overlay = nullptr;
lv_obj_t* s_lblName;
lv_obj_t* s_lblMin;

// Confirm: drop the oldest pending hop from the local queue. The
// controller-side queue will eventually subsume this (see hop-queue plan)
// and the local mirror disappears.
void on_confirm(lv_event_t*) {
    auto& s = app_state();
    size_t head = s.hopHead.load();
    size_t tail = s.hopTail.load();
    if (head != tail) {
        s.hopHead.store((head + 1) % AppState::MAX_HOPS);
    }
    // Notify controller too, in case it tracks ack server-side.
    ble_send("{\"k\":\"req:hop:ack\"}");
    hop_alert_hide();
}

void ensure_created() {
    if (s_overlay) return;
#ifdef USE_SQUARELINE_UI
    // ui_init() already created the overlay directly on the top layer
    // (LVGL v9 forbids reparenting screens). Just bind pointers + hide
    // by default + wire the Confirm button event.
    s_overlay  = ui_HopAlert;
    s_lblName  = ui_HopAlert_lblName;
    s_lblMin   = ui_HopAlert_lblMin;
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ui_HopAlert_btnConfirm, on_confirm, LV_EVENT_CLICKED, nullptr);
    return;
#else
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_overlay, 172, 320);
    lv_obj_align(s_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, theme_warning(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_overlay, 12, 0);
    lv_obj_set_flex_flow(s_overlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_overlay, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_overlay, 10, 0);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title = lv_label_create(s_overlay);
    lv_label_set_text(title, "ADICIONAR\nLUPULO");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_black(), 0);

    s_lblName = lv_label_create(s_overlay);
    lv_label_set_long_mode(s_lblName, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lblName, 140);
    lv_obj_set_style_text_align(s_lblName, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_lblName, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lblName, lv_color_black(), 0);
    lv_label_set_text(s_lblName, "");

    s_lblMin = lv_label_create(s_overlay);
    lv_obj_set_style_text_font(s_lblMin, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblMin, lv_color_black(), 0);
    lv_label_set_text(s_lblMin, "");

    lv_obj_t* btn = lv_button_create(s_overlay);
    lv_obj_set_size(btn, 130, 56);
    lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, on_confirm, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(btn);
    lv_label_set_text(bl, "CONFIRMAR");
    lv_obj_set_style_text_color(bl, theme_warning(), 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_center(bl);
#endif  // USE_SQUARELINE_UI
}

}  // namespace

void hop_alert_show(const char* name, int minMark) {
    ensure_created();
    lv_label_set_text(s_lblName, name);
    char buf[24];
    snprintf(buf, sizeof(buf), "marca: %d min", minMark);
    lv_label_set_text(s_lblMin, buf);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
}

void hop_alert_hide() {
    if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

bool hop_alert_visible() {
    return s_overlay && !lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

}}  // namespace inversa::display
