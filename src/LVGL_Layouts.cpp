#include "LVGL_Layouts.h"
#include "train_icon.h"

SemaphoreHandle_t lvgl_mutex = NULL;

lv_obj_t* header_bar;
lv_obj_t* main_tabview;

lv_obj_t* loco_tab;
lv_obj_t* acc_tab;
lv_obj_t* pwr_tab;
lv_obj_t* set_tab;

static lv_obj_t* wifi_label;
static lv_obj_t* cs_label;
static lv_obj_t* power_label;
static lv_obj_t* loco_label;

void setup_lvgl_layouts() {
    lvgl_mutex = xSemaphoreCreateMutex();

    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_pad_row(scr, 0, 0);
    lv_obj_set_style_pad_column(scr, 0, 0);

    // Top Header (30px height)
    header_bar = lv_obj_create(scr);
    lv_obj_set_size(header_bar, LV_PCT(100), 30);
    lv_obj_set_style_pad_all(header_bar, 0, 0);
    lv_obj_set_style_border_width(header_bar, 0, 0);
    lv_obj_set_flex_flow(header_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_style_pad_column(header_bar, 15, 0);

    lv_obj_t* loco_group = lv_obj_create(header_bar);
    lv_obj_set_size(loco_group, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_pad_all(loco_group, 0, 0);
    lv_obj_set_style_border_width(loco_group, 0, 0);
    lv_obj_set_style_bg_opa(loco_group, 0, 0);
    lv_obj_set_flex_flow(loco_group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(loco_group, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(loco_group, 5, 0); // Tighter gap
    lv_obj_clear_flag(loco_group, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* train_img = lv_image_create(loco_group);
    lv_image_set_src(train_img, &train_icon);
    lv_obj_set_style_text_color(train_img, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_pad_left(train_img, 10, 0);

    loco_label = lv_label_create(loco_group);
    lv_label_set_text(loco_label, "0");

    cs_label = lv_label_create(header_bar);
    lv_label_set_text(cs_label, LV_SYMBOL_LOOP);
    lv_obj_set_style_text_color(cs_label, lv_color_make(255, 0, 0), 0);

    lv_obj_t* spacer = lv_obj_create(header_bar);
    lv_obj_set_height(spacer, 0);
    lv_obj_set_style_bg_opa(spacer, 0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_flex_grow(spacer, 1);

    wifi_label = lv_label_create(header_bar);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(wifi_label, lv_color_make(255, 0, 0), 0);

    power_label = lv_label_create(header_bar);
    lv_label_set_text(power_label, LV_SYMBOL_BATTERY_FULL " --");
    lv_obj_set_style_pad_right(power_label, 10, 0);

    // Main Tabview (middle to bottom)
    main_tabview = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_position(main_tabview, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(main_tabview, 40);
    lv_obj_set_width(main_tabview, LV_PCT(100));
    lv_obj_set_flex_grow(main_tabview, 1); // Fills remaining vertical space
    
    // Remove padding to allow views to fill it seamlessly
    lv_obj_t* tab_btns = lv_tabview_get_tab_bar(main_tabview);
    lv_obj_set_style_pad_all(tab_btns, 0, 0);

    loco_tab = lv_tabview_add_tab(main_tabview, "Loco");
    acc_tab = lv_tabview_add_tab(main_tabview, "Acc");
    pwr_tab = lv_tabview_add_tab(main_tabview, "Pwr");
    set_tab = lv_tabview_add_tab(main_tabview, "Set");

    lv_obj_set_style_pad_all(loco_tab, 0, 0);
    lv_obj_set_style_pad_all(acc_tab, 0, 0);
    lv_obj_set_style_pad_all(pwr_tab, 0, 0);
    lv_obj_set_style_pad_all(set_tab, 0, 0);
}

void set_header_loco_count(int count) {
    if (loco_label) lv_label_set_text_fmt(loco_label, "%d", count);
}

void set_header_wifi_status(bool connected) {
    if (wifi_label) {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(wifi_label, connected ? lv_color_make(0, 255, 0) : lv_color_make(255, 0, 0), 0);
    }
}

void set_header_cs_status(bool connected) {
    if (cs_label) {
        lv_label_set_text(cs_label, LV_SYMBOL_LOOP);
        lv_obj_set_style_text_color(cs_label, connected ? lv_color_make(0, 255, 0) : lv_color_make(255, 0, 0), 0);
    }
}

void set_header_power_status(float voltage) {
    if (power_label) lv_label_set_text_fmt(power_label, LV_SYMBOL_BATTERY_FULL " %.2fV", voltage);
}
