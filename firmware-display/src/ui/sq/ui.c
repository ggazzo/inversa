// Inversa Display — hand-rolled LVGL v9 UI implementing the sq_ui.h
// contract. This replaces what SquareLine Studio 1.6.1 emits because
// that export is LVGL v8 and uses naming "Name" (no screen prefix),
// which collides across screens (lblTemp lives in Home, Recipe, Manual,
// Scheduled — all would resolve to a single `ui_lblTemp` global).
//
// Keep this file. Do NOT let Studio overwrite it again unless Studio is
// configured for LVGL 9 AND a screen-prefixed naming scheme.

#include "sq_ui.h"
#include <lvgl.h>

#define COL_BG        0x0B0B0F
#define COL_SURFACE   0x1A1A22
#define COL_PRIMARY   0xFFFFFF
#define COL_MUTED     0x6B7280
#define COL_ACCENT    0x10B981
#define COL_WARNING   0xF59E0B
#define COL_DANGER    0xEF4444
#define COL_INFO      0x3B82F6
#define COL_BLACK     0x000000

#define LCD_W 172
#define LCD_H 320

lv_obj_t* ui_Home;
lv_obj_t* ui_Home_lblTemp;
lv_obj_t* ui_Home_lblTarget;
lv_obj_t* ui_Home_lblMode;
lv_obj_t* ui_Home_lblClock;
lv_obj_t* ui_Home_arc;
lv_obj_t* ui_Home_dotHeater;
lv_obj_t* ui_Home_dotPump;

lv_obj_t* ui_Recipe;
lv_obj_t* ui_Recipe_lblName;
lv_obj_t* ui_Recipe_lblStep;
lv_obj_t* ui_Recipe_lblStepIdx;
lv_obj_t* ui_Recipe_lblTemp;
lv_obj_t* ui_Recipe_lblTarget;
lv_obj_t* ui_Recipe_bar;
lv_obj_t* ui_Recipe_lblBoil;
lv_obj_t* ui_Recipe_btnRow;
lv_obj_t* ui_Recipe_btnConfirm;
lv_obj_t* ui_Recipe_btnPause;
lv_obj_t* ui_Recipe_lblBtnPause;
lv_obj_t* ui_Recipe_btnStop;

lv_obj_t* ui_Manual;
lv_obj_t* ui_Manual_lblTemp;
lv_obj_t* ui_Manual_lblTarget;
lv_obj_t* ui_Manual_slider;
lv_obj_t* ui_Manual_switch;

lv_obj_t* ui_Scheduled;
lv_obj_t* ui_Scheduled_lblTarget;
lv_obj_t* ui_Scheduled_lblCountdown;
lv_obj_t* ui_Scheduled_lblTemp;
lv_obj_t* ui_Scheduled_lblVol;
lv_obj_t* ui_Scheduled_lblStatus;
lv_obj_t* ui_Scheduled_btnStop;

lv_obj_t* ui_HopAlert;
lv_obj_t* ui_HopAlert_lblName;
lv_obj_t* ui_HopAlert_lblMin;
lv_obj_t* ui_HopAlert_btnConfirm;

lv_obj_t* ui_Watchdog;
lv_obj_t* ui_Watchdog_lblCause;
lv_obj_t* ui_Watchdog_lblHard;
lv_obj_t* ui_Watchdog_btnReset;

lv_obj_t* ui_Scanning;

static lv_obj_t* mk_screen(uint32_t bg) {
    lv_obj_t* s = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s, 8, 0);
    lv_obj_set_style_border_width(s, 0, 0);
    lv_obj_clear_flag(s, LV_OBJ_FLAG_SCROLLABLE);
    return s;
}

static lv_obj_t* mk_label(lv_obj_t* parent, const lv_font_t* font,
                          uint32_t color, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    if (text) lv_label_set_text(l, text);
    return l;
}

static lv_obj_t* mk_button(lv_obj_t* parent, int w, int h, uint32_t bg) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    return b;
}

static lv_obj_t* mk_btn_with_label(lv_obj_t* parent, int w, int h,
                                   uint32_t bg, const char* text,
                                   uint32_t text_color,
                                   const lv_font_t* font) {
    lv_obj_t* b = mk_button(parent, w, h, bg);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, lv_color_hex(text_color), 0);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_center(l);
    return b;
}

static lv_obj_t* mk_row(lv_obj_t* parent) {
    lv_obj_t* r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return r;
}

static lv_obj_t* mk_status_dot(lv_obj_t* parent, const char* caption) {
    lv_obj_t* row = mk_row(parent);
    lv_obj_set_style_pad_column(row, 6, 0);

    lv_obj_t* dot = lv_obj_create(row);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(COL_SURFACE), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);

    mk_label(row, &lv_font_montserrat_14, COL_MUTED, caption);
    return dot;
}

static void build_home(void) {
    ui_Home = mk_screen(COL_BG);

    ui_Home_lblClock = mk_label(ui_Home, &lv_font_montserrat_14,
                                COL_MUTED, "--:--");
    lv_obj_align(ui_Home_lblClock, LV_ALIGN_TOP_MID, 0, 0);

    ui_Home_arc = lv_arc_create(ui_Home);
    lv_obj_set_size(ui_Home_arc, 150, 150);
    lv_obj_align(ui_Home_arc, LV_ALIGN_TOP_MID, 0, 22);
    lv_arc_set_range(ui_Home_arc, 0, 100);
    lv_arc_set_bg_angles(ui_Home_arc, 135, 45);
    lv_arc_set_value(ui_Home_arc, 0);
    lv_obj_remove_style(ui_Home_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ui_Home_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(ui_Home_arc,
        lv_color_hex(COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui_Home_arc,
        lv_color_hex(COL_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ui_Home_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui_Home_arc, 10, LV_PART_INDICATOR);

    ui_Home_lblTemp = mk_label(ui_Home, &lv_font_montserrat_48,
                               COL_PRIMARY, "--");
    lv_obj_align(ui_Home_lblTemp, LV_ALIGN_TOP_MID, 0, 72);

    ui_Home_lblTarget = mk_label(ui_Home, &lv_font_montserrat_14,
                                 COL_MUTED, "alvo --");
    lv_obj_align(ui_Home_lblTarget, LV_ALIGN_TOP_MID, 0, 134);

    ui_Home_lblMode = mk_label(ui_Home, &lv_font_montserrat_24,
                               COL_PRIMARY, "Idle");
    lv_obj_align(ui_Home_lblMode, LV_ALIGN_BOTTOM_MID, 0, -50);

    lv_obj_t* dots = mk_row(ui_Home);
    lv_obj_set_width(dots, LV_PCT(100));
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -12);
    ui_Home_dotHeater = mk_status_dot(dots, "heat");
    ui_Home_dotPump   = mk_status_dot(dots, "pump");
}

static void build_recipe(void) {
    ui_Recipe = mk_screen(COL_BG);
    lv_obj_set_flex_flow(ui_Recipe, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_Recipe, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui_Recipe, 4, 0);

    ui_Recipe_lblStepIdx = mk_label(ui_Recipe, &lv_font_montserrat_14,
                                    COL_MUTED, "Passo --/--");

    ui_Recipe_lblName = mk_label(ui_Recipe, &lv_font_montserrat_14,
                                 COL_MUTED, "");
    lv_label_set_long_mode(ui_Recipe_lblName, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ui_Recipe_lblName, 156);

    ui_Recipe_lblStep = mk_label(ui_Recipe, &lv_font_montserrat_24,
                                 COL_PRIMARY, "Passo");
    lv_label_set_long_mode(ui_Recipe_lblStep, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ui_Recipe_lblStep, 156);

    ui_Recipe_lblTemp = mk_label(ui_Recipe, &lv_font_montserrat_48,
                                 COL_PRIMARY, "--");

    ui_Recipe_lblTarget = mk_label(ui_Recipe, &lv_font_montserrat_14,
                                   COL_MUTED, "alvo --");

    ui_Recipe_bar = lv_bar_create(ui_Recipe);
    lv_obj_set_size(ui_Recipe_bar, 156, 6);
    lv_bar_set_range(ui_Recipe_bar, 0, 100);
    lv_bar_set_value(ui_Recipe_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_Recipe_bar,
        lv_color_hex(COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Recipe_bar,
        lv_color_hex(COL_ACCENT), LV_PART_INDICATOR);

    ui_Recipe_lblBoil = mk_label(ui_Recipe, &lv_font_montserrat_24,
                                 COL_WARNING, "");
    lv_obj_add_flag(ui_Recipe_lblBoil, LV_OBJ_FLAG_HIDDEN);

    ui_Recipe_btnRow = lv_obj_create(ui_Recipe);
    lv_obj_remove_style_all(ui_Recipe_btnRow);
    lv_obj_set_size(ui_Recipe_btnRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(ui_Recipe_btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_Recipe_btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(ui_Recipe_btnRow, LV_OBJ_FLAG_HIDDEN);

    ui_Recipe_btnConfirm = mk_btn_with_label(ui_Recipe_btnRow, 50, 36,
        COL_ACCENT, "OK", COL_PRIMARY, &lv_font_montserrat_14);
    ui_Recipe_btnPause = mk_btn_with_label(ui_Recipe_btnRow, 50, 36,
        COL_WARNING, "Pause", COL_PRIMARY, &lv_font_montserrat_14);
    ui_Recipe_lblBtnPause = lv_obj_get_child(ui_Recipe_btnPause, 0);
    ui_Recipe_btnStop = mk_btn_with_label(ui_Recipe_btnRow, 50, 36,
        COL_DANGER, "Stop", COL_PRIMARY, &lv_font_montserrat_14);
}

static void build_manual(void) {
    ui_Manual = mk_screen(COL_BG);
    lv_obj_set_style_pad_all(ui_Manual, 10, 0);
    lv_obj_set_flex_flow(ui_Manual, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_Manual, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    mk_label(ui_Manual, &lv_font_montserrat_14, COL_MUTED, "Manual");

    ui_Manual_lblTemp = mk_label(ui_Manual, &lv_font_montserrat_48,
                                 COL_PRIMARY, "--");

    ui_Manual_lblTarget = mk_label(ui_Manual, &lv_font_montserrat_24,
                                   COL_ACCENT, "\xE2\x86\x92 65\xC2\xB0");

    ui_Manual_slider = lv_slider_create(ui_Manual);
    lv_obj_set_size(ui_Manual_slider, 150, 22);
    lv_slider_set_range(ui_Manual_slider, 22, 100);
    lv_slider_set_value(ui_Manual_slider, 65, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui_Manual_slider,
        lv_color_hex(COL_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui_Manual_slider,
        lv_color_hex(COL_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui_Manual_slider,
        lv_color_hex(COL_PRIMARY), LV_PART_KNOB);

    lv_obj_t* row = mk_row(ui_Manual);
    lv_obj_set_style_pad_column(row, 8, 0);
    mk_label(row, &lv_font_montserrat_14, COL_MUTED, "Resistencia");
    ui_Manual_switch = lv_switch_create(row);
    lv_obj_set_size(ui_Manual_switch, 50, 26);
}

static void build_scheduled(void) {
    ui_Scheduled = mk_screen(COL_BG);
    lv_obj_set_style_pad_all(ui_Scheduled, 10, 0);
    lv_obj_set_flex_flow(ui_Scheduled, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_Scheduled, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui_Scheduled, 6, 0);

    lv_obj_t* title = mk_label(ui_Scheduled, &lv_font_montserrat_14,
                               COL_INFO, LV_SYMBOL_BELL "  Agendado");
    (void)title;

    ui_Scheduled_lblTarget = mk_label(ui_Scheduled, &lv_font_montserrat_48,
                                      COL_PRIMARY, "--:--");

    ui_Scheduled_lblCountdown = mk_label(ui_Scheduled, &lv_font_montserrat_24,
                                         COL_ACCENT, "--");

    ui_Scheduled_lblTemp = mk_label(ui_Scheduled, &lv_font_montserrat_14,
                                    COL_MUTED, "alvo --\xC2\xB0");

    ui_Scheduled_lblVol = mk_label(ui_Scheduled, &lv_font_montserrat_14,
                                   COL_MUTED, "vol --L");

    ui_Scheduled_lblStatus = mk_label(ui_Scheduled, &lv_font_montserrat_14,
                                      COL_WARNING, "");
    lv_label_set_long_mode(ui_Scheduled_lblStatus, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ui_Scheduled_lblStatus, 150);
    lv_obj_set_style_text_align(ui_Scheduled_lblStatus,
                                LV_TEXT_ALIGN_CENTER, 0);

    ui_Scheduled_btnStop = mk_btn_with_label(ui_Scheduled, 140, 44,
        COL_DANGER, "Cancelar", COL_PRIMARY, &lv_font_montserrat_14);
}

static void build_hop_alert(void) {
    // Top-layer child (not a screen). LVGL v9 forbids re-parenting
    // screens, so creating directly under lv_layer_top() lets the
    // bridge show/hide it without lv_obj_set_parent.
    ui_HopAlert = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ui_HopAlert, LCD_W, LCD_H);
    lv_obj_set_style_bg_color(ui_HopAlert, lv_color_hex(COL_WARNING), 0);
    lv_obj_set_style_bg_opa(ui_HopAlert, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_HopAlert, 0, 0);
    lv_obj_set_style_pad_all(ui_HopAlert, 12, 0);
    lv_obj_set_flex_flow(ui_HopAlert, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_HopAlert, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui_HopAlert, 10, 0);
    lv_obj_clear_flag(ui_HopAlert, LV_OBJ_FLAG_SCROLLABLE);
    // Start hidden — bridge calls hop_alert_show() when a notification
    // queues a hop. Without this, the overlay sits on top of the active
    // screen from boot and a transient watchdog overlay flicker reveals
    // it underneath, which looks like a screen change to the user.
    lv_obj_add_flag(ui_HopAlert, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title = mk_label(ui_HopAlert, &lv_font_montserrat_24,
                               COL_BLACK, "ADICIONAR\nLUPULO");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

    ui_HopAlert_lblName = mk_label(ui_HopAlert, &lv_font_montserrat_24,
                                   COL_BLACK, "");
    lv_label_set_long_mode(ui_HopAlert_lblName, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ui_HopAlert_lblName, 140);
    lv_obj_set_style_text_align(ui_HopAlert_lblName,
                                LV_TEXT_ALIGN_CENTER, 0);

    ui_HopAlert_lblMin = mk_label(ui_HopAlert, &lv_font_montserrat_14,
                                  COL_BLACK, "");

    ui_HopAlert_btnConfirm = mk_btn_with_label(ui_HopAlert, 130, 56,
        COL_BLACK, "CONFIRMAR", COL_WARNING, &lv_font_montserrat_14);
    lv_obj_set_style_radius(ui_HopAlert_btnConfirm, 8, 0);
}

static void build_watchdog(void) {
    ui_Watchdog = lv_obj_create(lv_layer_top());
    lv_obj_set_size(ui_Watchdog, LCD_W, LCD_H);
    lv_obj_set_style_bg_color(ui_Watchdog, lv_color_hex(COL_DANGER), 0);
    lv_obj_set_style_bg_opa(ui_Watchdog, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_Watchdog, 0, 0);
    lv_obj_set_style_pad_all(ui_Watchdog, 10, 0);
    lv_obj_set_flex_flow(ui_Watchdog, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_Watchdog, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ui_Watchdog, 8, 0);
    lv_obj_clear_flag(ui_Watchdog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_Watchdog, LV_OBJ_FLAG_HIDDEN);

    mk_label(ui_Watchdog, &lv_font_montserrat_48,
             COL_PRIMARY, LV_SYMBOL_WARNING);
    mk_label(ui_Watchdog, &lv_font_montserrat_24,
             COL_PRIMARY, "WATCHDOG");

    ui_Watchdog_lblCause = mk_label(ui_Watchdog, &lv_font_montserrat_14,
                                    COL_PRIMARY, "");
    lv_label_set_long_mode(ui_Watchdog_lblCause, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(ui_Watchdog_lblCause, 150);
    lv_obj_set_style_text_align(ui_Watchdog_lblCause,
                                LV_TEXT_ALIGN_CENTER, 0);

    ui_Watchdog_lblHard = mk_label(ui_Watchdog, &lv_font_montserrat_14,
                                   COL_PRIMARY, "");

    ui_Watchdog_btnReset = mk_btn_with_label(ui_Watchdog, 130, 44,
        COL_BLACK, "RESET", COL_DANGER, &lv_font_montserrat_14);
}

void ui_init(void) {
    build_home();
    build_recipe();
    build_manual();
    build_scheduled();
    build_hop_alert();
    build_watchdog();
    ui_Scanning = NULL;
}
