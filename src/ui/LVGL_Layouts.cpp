#include "LVGL_Layouts.h"
#include "train_icon.h"
#include "dcc_icon.h"

LV_FONT_DECLARE(fa_gauge_high_16);

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

// Custom dark theme override: strips the blue tint from LVGL's default dark palette.
// Runs AFTER the base theme so only bg/border colors on plain containers are changed.
static lv_theme_t* custom_dark_theme = nullptr;

static void dark_override_cb(lv_theme_t* th, lv_obj_t* obj) {
    LV_UNUSED(th);
    if (!lv_obj_check_type(obj, &lv_obj_class)) return;

    if (lv_obj_get_parent(obj) == NULL) {
        // Screen — pure near-black
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x0a0a0a), 0);
    } else {
        // Panel / container — neutral dark grey, no blue tint
        lv_obj_set_style_bg_color(obj, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_border_color(obj, lv_color_hex(0x333333), 0);
    }
}

void apply_theme() {
    bool is_dark = (Settings.theme == SettingsClass::Theme::DARK);
    lv_disp_t * disp = lv_disp_get_default();
    lv_theme_t * base = lv_theme_default_init(disp,
        lv_color_make(50, 150, 255),  /* Palette primary */
        lv_color_make(255, 50, 50),   /* Palette secondary */
        is_dark,
        &lv_font_montserrat_14);

    if (is_dark) {
        if (!custom_dark_theme) custom_dark_theme = lv_theme_create();
        lv_theme_set_parent(custom_dark_theme, base);
        lv_theme_set_apply_cb(custom_dark_theme, dark_override_cb);
        lv_disp_set_theme(disp, custom_dark_theme);
    } else {
        lv_disp_set_theme(disp, base);
    }

}

// Creates the header bar and all status widgets. Safe to call multiple times.
static void create_header_bar() {
    lv_obj_t* scr = lv_scr_act();

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
    lv_obj_set_style_pad_column(loco_group, 0, 0);
    lv_obj_clear_flag(loco_group, LV_OBJ_FLAG_SCROLLABLE);

    train_img = lv_image_create(loco_group);
    lv_image_set_src(train_img, &train_icon);
    lv_obj_set_style_pad_left(train_img, 10, 0);
    bool is_dark = (Settings.theme == SettingsClass::Theme::DARK);
    lv_obj_set_style_image_recolor_opa(train_img, LV_OPA_COVER, 0);
    lv_obj_set_style_image_recolor(train_img, is_dark ? lv_color_make(255, 255, 255) : lv_color_make(0, 0, 0), 0);

    loco_label = lv_label_create(train_img);
    lv_label_set_text(loco_label, "000");
    lv_obj_set_style_text_font(loco_label, &lv_font_montserrat_10, 0);
    lv_obj_align(loco_label, LV_ALIGN_BOTTOM_MID, -5, 1);

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

// Destroys and recreates the header bar so it picks up the current theme.
void rebuild_header_bar() {
    if (header_bar) {
        lv_obj_del(header_bar);
        header_bar  = nullptr;
        wifi_label  = nullptr;
        cs_icon     = nullptr;
        power_label = nullptr;
        loco_label  = nullptr;
        train_img   = nullptr;
    }
    create_header_bar();
}

// Called once at boot – creates the mutex and builds the header bar.
void setup_lvgl_layouts() {
    lvgl_mutex = xSemaphoreCreateMutex();

    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_pad_row(scr, 0, 0);
    lv_obj_set_style_pad_column(scr, 0, 0);

    create_header_bar();
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

    // UTF-8 for U+F625 gauge-high, U+F074 shuffle, U+F0E7 bolt, U+F013 gear
    loco_tab = lv_tabview_add_tab(main_tabview, "\xEF\x98\xA5 Loco");
    acc_tab  = lv_tabview_add_tab(main_tabview, "\xEF\x81\xB4 Acc");
    pwr_tab  = lv_tabview_add_tab(main_tabview, "\xEF\x83\xA7 Pwr");
    set_tab  = lv_tabview_add_tab(main_tabview, "\xEF\x80\x93 Set");

    lv_obj_set_style_text_font(tab_btns, &fa_gauge_high_16, 0);

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
        lv_color_t col = lv_color_make(255, 80, 80); // red — critically low
        if (voltage >= 4.10) { sym = LV_SYMBOL_BATTERY_FULL; col = lv_color_make(80, 220, 80);  }
        else if (voltage >= 3.90) { sym = LV_SYMBOL_BATTERY_3; col = lv_color_make(150, 220, 80); }
        else if (voltage >= 3.75) { sym = LV_SYMBOL_BATTERY_2; col = lv_color_make(220, 220, 80); }
        else if (voltage >= 3.60) { sym = LV_SYMBOL_BATTERY_1; col = lv_color_make(220, 140, 40); }

        char buf[16];
        snprintf(buf, sizeof(buf), "%s %.2fV", sym, voltage);
        lv_label_set_text(power_label, buf);
        lv_obj_set_style_text_color(power_label, col, 0);
    }
}
