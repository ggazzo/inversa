#include "Scheduled.h"
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
lv_obj_t* s_lblTarget;
lv_obj_t* s_lblTemp;
lv_obj_t* s_lblVol;
lv_obj_t* s_lblStatus;
lv_obj_t* s_lblCountdown;

void on_stop(lv_event_t*) { ble_send_sched_stop(); }

}  // namespace

lv_obj_t* scheduled_screen_create() {
#ifdef USE_SQUARELINE_UI
    s_scr           = ui_Scheduled;
    s_lblTarget     = ui_Scheduled_lblTarget;
    s_lblCountdown  = ui_Scheduled_lblCountdown;
    s_lblTemp       = ui_Scheduled_lblTemp;
    s_lblVol        = ui_Scheduled_lblVol;
    s_lblStatus     = ui_Scheduled_lblStatus;
    lv_obj_add_event_cb(ui_Scheduled_btnStop, on_stop, LV_EVENT_CLICKED, nullptr);
    return s_scr;
#else
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, theme_bg(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 10, 0);
    lv_obj_set_flex_flow(s_scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_scr, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_scr, 6, 0);

    lv_obj_t* title = lv_label_create(s_scr);
    lv_label_set_text(title, LV_SYMBOL_BELL "  Agendado");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, theme_info(), 0);

    s_lblTarget = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblTarget, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lblTarget, theme_primary(), 0);
    lv_label_set_text(s_lblTarget, "--:--");

    s_lblCountdown = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblCountdown, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lblCountdown, theme_accent(), 0);
    lv_label_set_text(s_lblCountdown, "--");

    s_lblTemp = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblTemp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblTemp, theme_muted(), 0);
    lv_label_set_text(s_lblTemp, "alvo --°");

    s_lblVol = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblVol, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblVol, theme_muted(), 0);
    lv_label_set_text(s_lblVol, "vol --L");

    s_lblStatus = lv_label_create(s_scr);
    lv_label_set_long_mode(s_lblStatus, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lblStatus, 150);
    lv_obj_set_style_text_align(s_lblStatus, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_lblStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblStatus, theme_warning(), 0);
    lv_label_set_text(s_lblStatus, "");

    lv_obj_t* btn = lv_button_create(s_scr);
    lv_obj_set_size(btn, 140, 44);
    lv_obj_set_style_bg_color(btn, theme_danger(), 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_add_event_cb(btn, on_stop, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* bl = lv_label_create(btn);
    lv_label_set_text(bl, "Cancelar");
    lv_obj_set_style_text_color(bl, theme_primary(), 0);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_center(bl);

    return s_scr;
#endif  // USE_SQUARELINE_UI
}

void scheduled_screen_update() {
    if (!s_scr) return;
    auto& s = app_state();
    char buf[40];

    snprintf(buf, sizeof(buf), "%02d:%02d",
             s.schedTargetHour.load(), s.schedTargetMin.load());
    lv_label_set_text(s_lblTarget, buf);

    snprintf(buf, sizeof(buf), "alvo %.1f°", s.schedTargetTemp.load());
    lv_label_set_text(s_lblTemp, buf);

    snprintf(buf, sizeof(buf), "vol %.1fL", s.schedVolume.load());
    lv_label_set_text(s_lblVol, buf);

    lv_label_set_text(s_lblStatus,
        s.schedStatus[0] ? s.schedStatus : "Aguardando");

    // Compute countdown from RTC vs target HH:MM (today, or tomorrow if
    // target already passed). Naive but matches the PWA presentation.
    if (s.rtcAvailable.load()) {
        uint32_t now = s.rtcUnix.load();
        uint32_t today = now - (now % 86400);
        uint32_t target = today
            + uint32_t(s.schedTargetHour.load()) * 3600
            + uint32_t(s.schedTargetMin.load()) * 60;
        if (target < now) target += 86400;
        uint32_t delta = target - now;
        int hh = delta / 3600;
        int mm = (delta / 60) % 60;
        snprintf(buf, sizeof(buf), "em %dh%02dm", hh, mm);
        lv_label_set_text(s_lblCountdown, buf);
    } else {
        lv_label_set_text(s_lblCountdown, "");
    }
}

}}  // namespace inversa::display
