#include "Scanning.h"

namespace inversa { namespace display {

lv_obj_t* scanning_screen_create() {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0B0B0F), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* col = lv_obj_create(scr);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, LV_PCT(100), LV_PCT(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 12, 0);

    lv_obj_t* dotIcon = lv_label_create(col);
    lv_label_set_text(dotIcon, LV_SYMBOL_BLUETOOTH);
    lv_obj_set_style_text_font(dotIcon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(dotIcon, lv_color_white(), 0);

    lv_obj_t* title = lv_label_create(col);
    lv_label_set_text(title, "Procurando\ncontrolador");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t* hint = lv_label_create(col);
    lv_label_set_text(hint, "Inversa BLE");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x6B7280), 0);

    lv_obj_t* spinner = lv_spinner_create(col);
    lv_obj_set_size(spinner, 48, 48);

    return scr;
}

}}  // namespace inversa::display
