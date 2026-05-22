#include "Recipe.h"
#include "Theme.h"
#include "../state/AppState.h"
#include "../ble/BleClient.h"
#ifdef USE_SQUARELINE_UI
#include "../ui/sq/sq_ui.h"
#endif
#include <cstdio>
#include <cstring>

namespace inversa { namespace display {

namespace {

lv_obj_t* s_scr = nullptr;
lv_obj_t* s_lblName;
lv_obj_t* s_lblStep;
lv_obj_t* s_lblStepIdx;
lv_obj_t* s_lblTemp;
lv_obj_t* s_lblTarget;
lv_obj_t* s_bar;
lv_obj_t* s_lblBoil;
lv_obj_t* s_btnRow;
lv_obj_t* s_btnConfirm;
lv_obj_t* s_btnPause;
lv_obj_t* s_btnStop;
lv_obj_t* s_lblBtnPause;

void on_confirm(lv_event_t*) { ble_send_recipe_confirm(); }
void on_pause(lv_event_t*) {
    if (strcmp(app_state().recipeState, "paused") == 0) ble_send_recipe_resume();
    else                                                ble_send_recipe_pause();
}
void on_stop(lv_event_t*)    { ble_send_recipe_stop(); }

lv_obj_t* make_btn(lv_obj_t* parent, const char* txt,
                   lv_color_t bg, lv_event_cb_t cb) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, 50, 36);
    lv_obj_set_style_bg_color(b, bg, 0);
    lv_obj_set_style_radius(b, 6, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, theme_primary(), 0);
    lv_obj_center(l);
    return b;
}

}  // namespace

lv_obj_t* recipe_screen_create() {
#ifdef USE_SQUARELINE_UI
    s_scr           = ui_Recipe;
    s_lblName       = ui_Recipe_lblName;
    s_lblStep       = ui_Recipe_lblStep;
    s_lblStepIdx    = ui_Recipe_lblStepIdx;
    s_lblTemp       = ui_Recipe_lblTemp;
    s_lblTarget     = ui_Recipe_lblTarget;
    s_bar           = ui_Recipe_bar;
    s_lblBoil       = ui_Recipe_lblBoil;
    s_btnRow        = ui_Recipe_btnRow;
    s_btnConfirm    = ui_Recipe_btnConfirm;
    s_btnPause      = ui_Recipe_btnPause;
    s_lblBtnPause   = ui_Recipe_lblBtnPause;
    s_btnStop       = ui_Recipe_btnStop;
    // Event callbacks live in this file regardless of who built the
    // widget tree — SquareLine handlers are not used.
    lv_obj_add_event_cb(s_btnConfirm, on_confirm, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(s_btnPause,   on_pause,   LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(s_btnStop,    on_stop,    LV_EVENT_CLICKED, nullptr);
    return s_scr;
#else
    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, theme_bg(), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_scr, 8, 0);
    lv_obj_set_flex_flow(s_scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_scr, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_scr, 6, 0);

    s_lblStepIdx = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblStepIdx, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblStepIdx, theme_muted(), 0);
    lv_label_set_text(s_lblStepIdx, "Passo --/--");

    s_lblName = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblName, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblName, theme_muted(), 0);
    lv_label_set_long_mode(s_lblName, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lblName, 156);
    lv_label_set_text(s_lblName, "");

    s_lblStep = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblStep, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lblStep, theme_primary(), 0);
    lv_label_set_long_mode(s_lblStep, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_lblStep, 156);
    lv_label_set_text(s_lblStep, "Passo");

    s_lblTemp = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblTemp, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(s_lblTemp, theme_primary(), 0);
    lv_label_set_text(s_lblTemp, "--");

    s_lblTarget = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblTarget, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lblTarget, theme_muted(), 0);
    lv_label_set_text(s_lblTarget, "alvo --");

    s_bar = lv_bar_create(s_scr);
    lv_obj_set_size(s_bar, 156, 6);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar, theme_surface(), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, theme_accent(), LV_PART_INDICATOR);

    s_lblBoil = lv_label_create(s_scr);
    lv_obj_set_style_text_font(s_lblBoil, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_lblBoil, theme_warning(), 0);
    lv_label_set_text(s_lblBoil, "");

    s_btnRow = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_btnRow);
    lv_obj_set_size(s_btnRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(s_btnRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_btnRow, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(s_btnRow, LV_OBJ_FLAG_HIDDEN);

    s_btnConfirm = make_btn(s_btnRow, "OK",     theme_accent(),  on_confirm);
    s_btnPause   = make_btn(s_btnRow, "Pause",  theme_warning(), on_pause);
    s_lblBtnPause = lv_obj_get_child(s_btnPause, 0);
    s_btnStop    = make_btn(s_btnRow, "Stop",   theme_danger(),  on_stop);

    return s_scr;
#endif  // USE_SQUARELINE_UI
}

void recipe_screen_update() {
    if (!s_scr) return;
    auto& s = app_state();
    char buf[40];

    lv_label_set_text(s_lblName, s.recipeName);

    int idx = s.recipeStep.load();
    int tot = s.recipeTotal.load();
    snprintf(buf, sizeof(buf), "Passo %d/%d", idx, tot);
    lv_label_set_text(s_lblStepIdx, buf);

    lv_label_set_text(s_lblStep,
        s.recipeStepName[0] ? s.recipeStepName : "—");

    snprintf(buf, sizeof(buf), "%.1f°", s.currentTemp.load());
    lv_label_set_text(s_lblTemp, buf);

    snprintf(buf, sizeof(buf), "alvo %.1f°", s.targetTemp.load());
    lv_label_set_text(s_lblTarget, buf);

    // Boil substate: replace progress bar with mm:ss countdown.
    if (s.boilActive.load()) {
        int rem = s.boilRemaining.load();
        int mm = rem / 60;
        int ss = rem % 60;
        snprintf(buf, sizeof(buf), "Fervura %02d:%02d", mm, ss);
        lv_label_set_text(s_lblBoil, buf);
        lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_lblBoil, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_lblBoil, LV_OBJ_FLAG_HIDDEN);
        int pct = tot > 0 ? (idx * 100 / tot) : 0;
        lv_bar_set_value(s_bar, pct, LV_ANIM_OFF);
    }

    // Buttons appear only when the controller asks for input or is running.
    const char* rs = s.recipeState;
    // Controller's RecipeState enum stringifies via BLEPlugin.h:
    //   idle / running / paused / waiting_temp / waiting_timer /
    //   waiting_confirm / completed
    bool wait_confirm = s.waitingForConfirm.load() ||
                        strcmp(rs, "waiting_confirm") == 0;
    bool paused       = strcmp(rs, "paused") == 0;
    bool running      = strcmp(rs, "running") == 0 ||
                        strcmp(rs, "waiting_temp") == 0 ||
                        strcmp(rs, "waiting_timer") == 0 ||
                        paused || wait_confirm;
    if (running) lv_obj_clear_flag(s_btnRow, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(s_btnRow,   LV_OBJ_FLAG_HIDDEN);

    if (wait_confirm) lv_obj_clear_flag(s_btnConfirm, LV_OBJ_FLAG_HIDDEN);
    else              lv_obj_add_flag(s_btnConfirm,   LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(s_lblBtnPause, paused ? "Play" : "Pause");
}

}}  // namespace inversa::display
