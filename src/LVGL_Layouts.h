#pragma once
#include <LVGL_CYD.h>

extern lv_obj_t* header_bar;
extern lv_obj_t* content_area;
extern lv_obj_t* bottom_bar;

void setup_lvgl_layouts();
void set_header_loco_count(int count);
void set_header_wifi_status(bool connected);
void set_header_cs_status(bool connected);
void set_header_power_status(float voltage);
