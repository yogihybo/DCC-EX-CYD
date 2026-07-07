#pragma once

#include <LVGL_CYD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t lvgl_mutex;

class UIView {
    public:
        virtual ~UIView() = default;
};

// Low-battery thresholds (LiPo, volts). At/below LOW_BATTERY_VOLTS the header
// battery indicator turns red and the backlight is auto-dimmed to conserve
// power; it clears once voltage recovers past LOW_BATTERY_CLEAR_VOLTS (hysteresis).
#define LOW_BATTERY_VOLTS        3.60f
#define LOW_BATTERY_CLEAR_VOLTS  3.75f

extern lv_obj_t* header_bar;
extern lv_obj_t* main_tabview;

extern lv_obj_t* loco_tab;
extern lv_obj_t* acc_tab;
extern lv_obj_t* pwr_tab;
extern lv_obj_t* set_tab;

// Called once at boot to create the mutex and header bar
void setup_lvgl_layouts();

// Destroys and recreates the header bar with the current theme
void rebuild_header_bar();

// Creates (or recreates) the main tabview and its four tabs.
// Called at boot and again after exiting Throttle Programming mode.
void create_main_ui();

void set_header_loco_count(int count);
void set_header_wifi_status(bool connected, int rssi);
void set_header_cs_status(bool connected);
void set_header_power_status(float voltage);

void apply_theme();
