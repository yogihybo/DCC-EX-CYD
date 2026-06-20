#include "LVGL_Layouts.h"

LV_FONT_DECLARE(fa_gauge_high_16);
LV_FONT_DECLARE(fa_icons_18);

SemaphoreHandle_t lvgl_mutex = NULL;

lv_obj_t* header_bar;
lv_obj_t* main_tabview;

lv_obj_t* loco_tab;
lv_obj_t* acc_tab;
lv_obj_t* pwr_tab;
lv_obj_t* set_tab;

static lv_obj_t* wifi_bars[4];
static lv_obj_t* cs_wave;
static lv_obj_t* power_label;
static lv_obj_t* voltage_label;
static lv_obj_t* loco_label;
static lv_obj_t* train_img;

static lv_obj_t* make_signal_bars(lv_obj_t* parent) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 20, 18);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    static const uint8_t heights[4] = { 5, 8, 11, 14 };
    for (int i = 0; i < 4; i++) {
        lv_obj_t* bar = lv_obj_create(cont);
        lv_obj_set_size(bar, 3, heights[i]);
        lv_obj_set_pos(bar, i * 5, 18 - heights[i]);
        lv_obj_set_style_radius(bar, 1, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x555555), 0);
    }
    return cont;
}

// DCC square-wave icon drawn as a single lv_line polyline.
// 4 pulses across 22px with a 2px horizontal lead-in and trail so the line
// starts and ends with a visible horizontal segment rather than a vertical edge.
static lv_obj_t* make_dcc_waveform(lv_obj_t* parent) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 22, 18);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_opa(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // 2px low lead → 4 pulses (4px high + 4px low each) → 2px high trail = 22px total
    static const lv_point_precise_t pts[] = {
        { 0, 14}, { 2, 14}, { 2,  4}, { 6,  4}, { 6, 14},
        {10, 14}, {10,  4}, {14,  4}, {14, 14},
        {18, 14}, {18,  4}, {22,  4},
    };
    lv_obj_t* line = lv_line_create(cont);
    lv_line_set_points(line, pts, sizeof(pts) / sizeof(pts[0]));
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(0x555555), 0);
    lv_obj_set_pos(line, 0, 0);
    return cont;
}

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
    lv_obj_set_flex_align(header_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_column(header_bar, 15, 0);

    lv_obj_t* loco_group = lv_obj_create(header_bar);
    lv_obj_set_size(loco_group, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_pad_all(loco_group, 0, 0);
    lv_obj_set_style_border_width(loco_group, 0, 0);
    lv_obj_set_style_bg_opa(loco_group, 0, 0);
    lv_obj_set_flex_flow(loco_group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(loco_group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(loco_group, 0, 0);
    lv_obj_clear_flag(loco_group, LV_OBJ_FLAG_SCROLLABLE);

    train_img = lv_label_create(loco_group);
    lv_label_set_text(train_img, "\xEF\x88\xB8");  // U+F238 fa-train
    lv_obj_set_style_text_font(train_img, &fa_icons_18, 0);
    lv_obj_set_style_text_color(train_img, lv_color_hex(0xcccccc), 0);
    lv_obj_set_style_pad_left(train_img, 10, 0);
    lv_obj_set_style_pad_top(train_img, -4, 0);           // compress flex layout height
    lv_obj_set_style_pad_bottom(train_img, -4, 0);
    lv_obj_set_style_transform_scale(train_img, 177, 0);  // 69% of original
    lv_obj_set_style_transform_pivot_x(train_img, 0, 0);
    lv_obj_set_style_transform_pivot_y(train_img, 9, 0);

    loco_label = lv_label_create(loco_group);
    lv_label_set_text(loco_label, "000");
    lv_obj_set_style_text_font(loco_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(loco_label, lv_color_hex(0x999999), 0);
    lv_obj_set_style_translate_y(loco_label, 3, 0);

    // CS signal bars + "DCC" label stacked in a column
    lv_obj_t* cs_group = lv_obj_create(header_bar);
    lv_obj_set_size(cs_group, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_pad_all(cs_group, 0, 0);
    lv_obj_set_style_border_width(cs_group, 0, 0);
    lv_obj_set_style_bg_opa(cs_group, 0, 0);
    lv_obj_set_flex_flow(cs_group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cs_group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(cs_group, 0, 0);
    lv_obj_clear_flag(cs_group, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* cs_cont = make_dcc_waveform(cs_group);
    cs_wave = lv_obj_get_child(cs_cont, 0); // the lv_line
    lv_obj_t* cs_lbl = lv_label_create(cs_group);
    lv_label_set_text(cs_lbl, "DCC");
    lv_obj_set_style_text_font(cs_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(cs_lbl, lv_color_hex(0x999999), 0);

    lv_obj_t* spacer = lv_obj_create(header_bar);
    lv_obj_set_height(spacer, 0);
    lv_obj_set_style_bg_opa(spacer, 0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_flex_grow(spacer, 1);

    // WiFi signal bars + "WiFi" label stacked in a column
    lv_obj_t* wifi_group = lv_obj_create(header_bar);
    lv_obj_set_size(wifi_group, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_pad_all(wifi_group, 0, 0);
    lv_obj_set_style_border_width(wifi_group, 0, 0);
    lv_obj_set_style_bg_opa(wifi_group, 0, 0);
    lv_obj_set_flex_flow(wifi_group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(wifi_group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(wifi_group, 0, 0);
    lv_obj_clear_flag(wifi_group, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* wifi_cont = make_signal_bars(wifi_group);
    for (int i = 0; i < 4; i++) wifi_bars[i] = lv_obj_get_child(wifi_cont, i);
    lv_obj_t* wifi_lbl = lv_label_create(wifi_group);
    lv_label_set_text(wifi_lbl, "WiFi");
    lv_obj_set_style_text_font(wifi_lbl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(wifi_lbl, lv_color_hex(0x999999), 0);

    // Battery icon + voltage label stacked in a column
    lv_obj_t* pwr_group = lv_obj_create(header_bar);
    lv_obj_set_size(pwr_group, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_style_pad_all(pwr_group, 0, 0);
    lv_obj_set_style_pad_right(pwr_group, 8, 0);
    lv_obj_set_style_border_width(pwr_group, 0, 0);
    lv_obj_set_style_bg_opa(pwr_group, 0, 0);
    lv_obj_set_flex_flow(pwr_group, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(pwr_group, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(pwr_group, 0, 0);
    lv_obj_clear_flag(pwr_group, LV_OBJ_FLAG_SCROLLABLE);

    power_label = lv_label_create(pwr_group);
    lv_label_set_text(power_label, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_font(power_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(power_label, lv_color_hex(0xcccccc), 0);

    voltage_label = lv_label_create(pwr_group);
    lv_label_set_text(voltage_label, "--");
    lv_obj_set_style_text_font(voltage_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(voltage_label, lv_color_hex(0x999999), 0);
}

// Destroys and recreates the header bar so it picks up the current theme.
void rebuild_header_bar() {
    if (header_bar) {
        lv_obj_del(header_bar);
        header_bar  = nullptr;
        power_label   = nullptr;
        voltage_label = nullptr;
        loco_label    = nullptr;
        train_img   = nullptr;
        for (int i = 0; i < 4; i++) wifi_bars[i] = nullptr;
        cs_wave = nullptr;
    }
    create_header_bar();
}

// Called once at boot – creates the mutex and builds the header bar.
// CYD rear RGB LED — active LOW, all off by default
#define LED_R 4
#define LED_G 16
#define LED_B 17

static void init_rear_led() {
    pinMode(LED_R, OUTPUT); digitalWrite(LED_R, HIGH);
    pinMode(LED_G, OUTPUT); digitalWrite(LED_G, HIGH);
    pinMode(LED_B, OUTPUT); digitalWrite(LED_B, HIGH);
}

void setup_lvgl_layouts() {
    init_rear_led();
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
    
    lv_obj_clear_flag(lv_tabview_get_content(main_tabview), LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* tab_btns = lv_tabview_get_tab_bar(main_tabview);
    lv_obj_set_style_pad_all(tab_btns, 0, 0);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x1a1a1a), 0);
    lv_obj_set_style_border_width(tab_btns, 0, 0);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(tab_btns, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(tab_btns, 1, 0);

    // UTF-8 for U+F625 gauge-high, U+F074 shuffle, U+F0E7 bolt, U+F013 gear
    loco_tab = lv_tabview_add_tab(main_tabview, "\xEF\x98\xA5 Loco");
    acc_tab  = lv_tabview_add_tab(main_tabview, "\xEF\x81\xB4 Acc");
    pwr_tab  = lv_tabview_add_tab(main_tabview, "\xEF\x83\xA7 Pwr");
    set_tab  = lv_tabview_add_tab(main_tabview, "\xEF\x80\x93 Set");

    lv_obj_set_style_text_font(tab_btns, &fa_gauge_high_16, 0);

    // Style individual tab buttons: grey inactive, blue active
    uint32_t tab_count = lv_obj_get_child_cnt(tab_btns);
    for (uint32_t i = 0; i < tab_count; i++) {
        lv_obj_t* btn = lv_obj_get_child(tab_btns, i);
        // Lock all padding for both states so the theme can't shift content on activation
        lv_obj_set_style_pad_top(btn,    2, 0);
        lv_obj_set_style_pad_bottom(btn, 0, 0);
        lv_obj_set_style_pad_left(btn,   0, 0);
        lv_obj_set_style_pad_right(btn,  0, 0);
        lv_obj_set_style_pad_top(btn,    2, LV_STATE_CHECKED);
        lv_obj_set_style_pad_bottom(btn, 0, LV_STATE_CHECKED);
        lv_obj_set_style_pad_left(btn,   0, LV_STATE_CHECKED);
        lv_obj_set_style_pad_right(btn,  0, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1a1a), 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1a1a1a), LV_STATE_CHECKED);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_color(btn, lv_color_hex(0x4488ff), LV_STATE_CHECKED);
        lv_obj_set_style_border_width(btn, 2, 0);
        lv_obj_set_style_border_width(btn, 2, LV_STATE_CHECKED);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_TOP, LV_STATE_CHECKED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x4488ff), LV_STATE_CHECKED);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, LV_STATE_CHECKED);
    }

    lv_obj_set_style_pad_all(loco_tab, 0, 0);
    lv_obj_set_style_pad_all(acc_tab,  0, 0);
    lv_obj_set_style_pad_all(pwr_tab,  0, 0);
    lv_obj_set_style_pad_all(set_tab,  0, 0);
}

void set_header_loco_count(int count) {
    if (loco_label) lv_label_set_text_fmt(loco_label, "%03d", count);
}

static void fill_bars(lv_obj_t* bars[4], int level, lv_color_t on_color) {
    for (int i = 0; i < 4; i++)
        if (bars[i]) lv_obj_set_style_bg_color(bars[i], i < level ? on_color : lv_color_hex(0x555555), 0);
}

void set_header_wifi_status(bool connected, int rssi) {
    int level = 0;
    if (connected) {
        if      (rssi > -60) level = 4;
        else if (rssi > -70) level = 3;
        else if (rssi > -80) level = 2;
        else                 level = 1;
    }
    fill_bars(wifi_bars, level, lv_color_hex(0xcccccc));
}

void set_header_cs_status(bool connected) {
    if (cs_wave)
        lv_obj_set_style_line_color(cs_wave, connected ? lv_color_hex(0xcccccc) : lv_color_hex(0x555555), 0);
    // Rear LED: green when connected, red otherwise (active LOW)
    digitalWrite(LED_G, connected ? LOW : HIGH);
    digitalWrite(LED_R, connected ? HIGH : LOW);
}

void set_header_power_status(float voltage) {
    if (power_label) {
        const char* sym = LV_SYMBOL_BATTERY_EMPTY;
        if      (voltage >= 4.10) sym = LV_SYMBOL_BATTERY_FULL;
        else if (voltage >= 3.90) sym = LV_SYMBOL_BATTERY_3;
        else if (voltage >= 3.75) sym = LV_SYMBOL_BATTERY_2;
        else if (voltage >= 3.60) sym = LV_SYMBOL_BATTERY_1;
        lv_label_set_text(power_label, sym);
    }
    if (voltage_label) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.2fV", voltage);
        lv_label_set_text(voltage_label, buf);
    }
}
