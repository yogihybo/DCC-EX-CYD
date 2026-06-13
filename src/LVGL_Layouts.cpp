#include "LVGL_Layouts.h"

lv_obj_t* header_bar;
lv_obj_t* content_area;
lv_obj_t* bottom_bar;

static lv_obj_t* wifi_label;
static lv_obj_t* cs_label;
static lv_obj_t* power_label;
static lv_obj_t* loco_label;

void setup_lvgl_layouts() {
    // Top Header (30px height)
    header_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(header_bar, LV_PCT(100), 30);
    lv_obj_align(header_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_flex_flow(header_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(header_bar, LV_OBJ_FLAG_SCROLLABLE);

    power_label = lv_label_create(header_bar);
    lv_label_set_text(power_label, "Bat: --");

    wifi_label = lv_label_create(header_bar);
    lv_label_set_text(wifi_label, "WiFi: X");

    cs_label = lv_label_create(header_bar);
    lv_label_set_text(cs_label, "CS: X");

    loco_label = lv_label_create(header_bar);
    lv_label_set_text(loco_label, "Locos: 0");

    // Content Area (middle)
    content_area = lv_obj_create(lv_scr_act());
    lv_obj_set_size(content_area, LV_PCT(100), 250);
    lv_obj_align(content_area, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t * temp_label = lv_label_create(content_area);
    lv_label_set_text(temp_label, "LocoUI Placeholder");
    lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, 0);

    // Bottom Bar (40px height)
    bottom_bar = lv_obj_create(lv_scr_act());
    lv_obj_set_size(bottom_bar, LV_PCT(100), 40);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(bottom_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(bottom_bar, LV_OBJ_FLAG_SCROLLABLE);

    const char* btns[] = {"Loco", "Acc", "Pwr", "Set"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* btn = lv_btn_create(bottom_bar);
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, btns[i]);
    }
}

void set_header_loco_count(int count) {
    if (loco_label) lv_label_set_text_fmt(loco_label, "Locos: %d", count);
}

void set_header_wifi_status(bool connected) {
    if (wifi_label) lv_label_set_text(wifi_label, connected ? "WiFi: OK" : "WiFi: X");
}

void set_header_cs_status(bool connected) {
    if (cs_label) lv_label_set_text(cs_label, connected ? "CS: OK" : "CS: X");
}

void set_header_power_status(float voltage) {
    if (power_label) lv_label_set_text_fmt(power_label, "%.2fV", voltage);
}
