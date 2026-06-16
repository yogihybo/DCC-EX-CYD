#include "LVGL_Layouts.h"
#include "train_icon.h"
#include "dcc_icon.h"

SemaphoreHandle_t lvgl_mutex = NULL;

lv_obj_t* header_bar;
lv_obj_t* main_tabview;

lv_obj_t* loco_tab;
lv_obj_t* acc_tab;
lv_obj_t* pwr_tab;
lv_obj_t* set_tab;

static lv_obj_t* wifi_label;
static lv_obj_t* cs_icon;
static lv_obj_t* power_label;
static lv_obj_t* loco_label;
static lv_obj_t* train_img;

#include <Settings.h>

void apply_theme() {
    bool is_dark = (Settings.theme == SettingsClass::Theme::DARK);
    lv_disp_t * disp = lv_disp_get_default();
    lv_theme_t * th = lv_theme_default_init(disp, 
        lv_color_make(50, 150, 255),  /* Palette primary */
        lv_color_make(255, 50, 50),   /* Palette secondary */
        is_dark, 
        &lv_font_montserrat_14);
    lv_disp_set_theme(disp, th);

    if (train_img) {
        lv_obj_set_style_image_recolor_opa(train_img, LV_OPA_COVER, 0);
        lv_obj_set_style_image_recolor(train_img, is_dark ? lv_color_make(255, 255, 255) : lv_color_make(0, 0, 0), 0);
    }
}

// Called once at boot – sets up the mutex and the fixed header bar.
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
    lv_obj_set_style_pad_column(loco_group, 0, 0); // Tighter gap
    lv_obj_clear_flag(loco_group, LV_OBJ_FLAG_SCROLLABLE);

    train_img = lv_image_create(loco_group);
    lv_image_set_src(train_img, &train_icon);
    lv_obj_set_style_pad_left(train_img, 10, 0);
    bool is_dark = (Settings.theme == SettingsClass::Theme::DARK);
    lv_obj_set_style_image_recolor_opa(train_img, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(train_img, is_dark ? lv_color_make(255, 255, 255) : lv_color_make(0, 0, 0), 0);

    // Make loco_label a child of the icon itself so we can overlay it
    loco_label = lv_label_create(train_img);
    lv_label_set_text(loco_label, "000");
    lv_obj_set_style_text_font(loco_label, &lv_font_montserrat_10, 0);
    lv_obj_align(loco_label, LV_ALIGN_BOTTOM_MID, -5, 1); // Move text left 5 pixels, up 3 pixels

    cs_icon = lv_image_create(header_bar);
    lv_image_set_src(cs_icon, &dcc_icon);
    lv_obj_set_style_image_recolor_opa(cs_icon, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(cs_icon, lv_color_make(255, 0, 0), 0);

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
}

// Creates (or recreates) the main tabview and its four tabs.
// Safe to call multiple times – each call builds a fresh tabview on lv_scr_act().
void create_main_ui() {
    lv_obj_t* scr = lv_scr_act();

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
    acc_tab  = lv_tabview_add_tab(main_tabview, "Acc");
    pwr_tab  = lv_tabview_add_tab(main_tabview, "Pwr");
    set_tab  = lv_tabview_add_tab(main_tabview, "Set");

    lv_obj_set_style_pad_all(loco_tab, 0, 0);
    lv_obj_set_style_pad_all(acc_tab,  0, 0);
    lv_obj_set_style_pad_all(pwr_tab,  0, 0);
    lv_obj_set_style_pad_all(set_tab,  0, 0);
}

void set_header_loco_count(int count) {
    if (loco_label) lv_label_set_text_fmt(loco_label, "%03d", count);
}

void set_header_wifi_status(bool connected, int rssi) {
    if (wifi_label) {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
        if (!connected) {
            lv_obj_set_style_text_color(wifi_label, lv_color_make(255, 0, 0), 0);
        } else {
            if (rssi > -60) lv_obj_set_style_text_color(wifi_label, lv_color_make(50, 255, 50), 0);
            else if (rssi > -80) lv_obj_set_style_text_color(wifi_label, lv_color_make(255, 255, 50), 0);
            else lv_obj_set_style_text_color(wifi_label, lv_color_make(255, 50, 50), 0);
        }
    }
}

void set_header_cs_status(bool connected) {
    if (cs_icon) {
        lv_obj_set_style_image_recolor(cs_icon, connected ? lv_color_make(0, 255, 0) : lv_color_make(255, 0, 0), 0);
    }
}

void set_header_power_status(float voltage) {
    if (power_label) {
        const char* sym = LV_SYMBOL_BATTERY_EMPTY;
        if (voltage >= 4.10) sym = LV_SYMBOL_BATTERY_FULL;
        else if (voltage >= 3.90) sym = LV_SYMBOL_BATTERY_3;
        else if (voltage >= 3.75) sym = LV_SYMBOL_BATTERY_2;
        else if (voltage >= 3.60) sym = LV_SYMBOL_BATTERY_1;
        
        char buf[16];
        snprintf(buf, sizeof(buf), "%s %.2fV", sym, voltage);
        lv_label_set_text(power_label, buf);
    }
}
