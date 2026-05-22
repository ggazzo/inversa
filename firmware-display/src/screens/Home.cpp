#include "Home.h"
#include "Theme.h"
#include "../state/AppState.h"
#ifdef USE_SQUARELINE_UI
#include "../ui/sq/sq_ui.h"
#endif
#include <cstdio>
#include <cstring>

namespace inversa { namespace display {

namespace {

lv_obj_t* s_scr = nullptr;
lv_obj_t* s_lblTemp;
lv_obj_t* s_lblTarget;
lv_obj_t* s_lblMode;
lv_obj_t* s_arc;
lv_obj_t* s_dotHeater;
lv_obj_t* s_dotPump;
lv_obj_t* s_lblClock;

lv_obj_t* status_dot(lv_obj_t* parent, const char* label) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);

    lv_obj_t* dot = lv_obj_create(row);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, theme_muted(), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, theme_muted(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    return dot;
}

}  // namespace

lv_obj_t* home_screen_create() {
#ifdef USE_SQUARELINE_UI
    // SquareLine path: ui_init() (called from bridge_init) already
    // allocated the screen and widgets. Just bind the local pointers
    // to the externs. Layout, fonts, colors are owned by the .c export
    // — this file does zero styling in the SquareLine branch.
    s_scr        = ui_Home;
    s_lblTemp    = ui_Home_lblTemp;
    s_lblTarget  = ui_Home_lblTarget;
    s_lblMode    = ui_Home_lblMode;
    s_lblClock   = ui_Home_lblClock;
    s_arc        = ui_Home_arc;
    s_dotHeater  = ui_Home_dotHeater;
    s_dotPump    = ui_Home_dotPump;
    return s_scr;
#else
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, theme_bg(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);

    // Top: clock chip
    s_lblClock = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblClock, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblClock, theme_muted(), 0);
    lv_obj_align(s_lblClock, LV_ALIGN_TOP_MID, 0, 6);
    lv_label_set_text(s_lblClock, "--:--");

    // Center: arc + temperature
    s_arc = lv_arc_create(s_scr);
    lv_obj_set_size(s_arc, 150, 150);
    lv_obj_align(s_arc, LV_ALIGN_TOP_MID, 0, 28);
    lv_arc_set_range(s_arc, 0, 100);
    lv_arc_set_bg_angles(s_arc, 135, 45);  // open at bottom
    lv_arc_set_value(s_arc, 0);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(s_arc, theme_surface(), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, theme_accent(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 10, LV_PART_INDICATOR);

    s_lblTemp = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblTemp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lblTemp, theme_primary(), 0);
    lv_obj_align(s_lblTemp, LV_ALIGN_TOP_MID, 0, 78);
    lv_label_set_text(s_lblTemp, "--");

    s_lblTarget = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblTarget, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblTarget, theme_muted(), 0);
    lv_obj_align(s_lblTarget, LV_ALIGN_TOP_MID, 0, 140);
    lv_label_set_text(s_lblTarget, "alvo --");

    // Bottom: mode label
    s_lblMode = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblMode, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lblMode, theme_primary(), 0);
    lv_obj_align(s_lblMode, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_label_set_text(s_lblMode, "Idle");

    // Status dots row
    lv_obj_t* dotsRow = lv_obj_create(s_scr);
    lv_obj_remove_style_all(dotsRow);
    lv_obj_set_size(dotsRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(dotsRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dotsRow, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(dotsRow, LV_ALIGN_BOTTOM_MID, 0, -12);

    s_dotHeater = status_dot(dotsRow, "heat");
    s_dotPump   = status_dot(dotsRow, "pump");

    return s_scr;
#endif  // USE_SQUARELINE_UI
}

void home_screen_update() {
    if (!s_scr) return;
    auto& s = app_state();
    char buf[24];

    float tc = s.currentTemp.load();
    float tg = s.targetTemp.load();

    snprintf(buf, sizeof(buf), "%.1f°", tc);
    lv_label_set_text(s_lblTemp, buf);

    snprintf(buf, sizeof(buf), "alvo %.1f°", tg);
    lv_label_set_text(s_lblTarget, buf);

    if (tg > 22.0f) {
        int pct = int(((tc - 22.0f) / (tg - 22.0f)) * 100.0f);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        lv_arc_set_value(s_arc, pct);
    }

    // Mode label, friendly cased.
    const char* mode = s.mode;
    const char* shown = "Idle";
    if      (strcmp(mode, "manual")   == 0) shown = "Manual";
    else if (strcmp(mode, "recipe")   == 0) shown = "Receita";
    else if (strcmp(mode, "autotune") == 0) shown = "Autotune";
    lv_label_set_text(s_lblMode, shown);

    lv_color_t accent = theme_accent();
    lv_color_t off    = theme_surface();
    lv_obj_set_style_bg_color(s_dotHeater,
        s.heaterOn.load() ? theme_danger() : off, 0);
    lv_obj_set_style_bg_color(s_dotPump,
        s.pumpOn.load() ? theme_info() : off, 0);

    if (s.rtcAvailable.load()) {
        uint32_t u = s.rtcUnix.load();
        uint32_t h = (u / 3600) % 24;
        uint32_t m = (u / 60) % 60;
        snprintf(buf, sizeof(buf), "%02u:%02u%s", h, m,
                 s.rtcNtpSynced.load() ? "" : "*");
        lv_label_set_text(s_lblClock, buf);
    } else {
        lv_label_set_text(s_lblClock, "");
    }
    (void)accent;
}

}}  // namespace inversa::display
