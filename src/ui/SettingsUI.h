#pragma once

#include <LVGL_CYD.h>
#include <Settings.h>
#include <DCCEXProtocol.h>
#include "LVGL_Layouts.h"
#include "CalibrationUI.h"

class SettingsUI : public UIView {
  private:
    lv_obj_t* _container;
    DCCEXProtocol& _dccex;

    class WiFiUI* _wifiUI;
    class AboutUI* _aboutUI;
    class CalibrationUI* _calibrationUI;
    class ProgramUI* _programUI;

    lv_obj_t* _speedStepLbl;
    lv_obj_t* _accelLbl;
    lv_obj_t* _rotationLbl;
    lv_obj_t* _themeLbl;
    lv_obj_t* _storageModeLbl;

    lv_obj_t* _brightnessLbl;
    lv_obj_t* _autoDimLbl;
    lv_obj_t* _eStopDelayLbl;
    lv_obj_t* _formatMsgbox = nullptr;
    lv_obj_t* _calMsgbox = nullptr;
    // lv_obj_t* _demoMsgbox = nullptr;

    lv_obj_t* _apModeLbl;
    static void ap_mode_event_cb(lv_event_t * e);

    static void speed_step_event_cb(lv_event_t * e);
    static void accel_event_cb(lv_event_t * e);
    static void rotation_event_cb(lv_event_t * e);
    static void theme_event_cb(lv_event_t * e);
    static void storage_mode_event_cb(lv_event_t * e);
    static void sd_format_event_cb(lv_event_t * e);
    static void sd_format_confirm_event_cb(lv_event_t * e);
    static void estop_delay_event_cb(lv_event_t * e);
    static void brightness_btn_event_cb(lv_event_t * e);
    static void brightness_event_cb(lv_event_t * e);
    static void auto_dim_event_cb(lv_event_t * e);
    static void wifi_setup_event_cb(lv_event_t * e);
    static void about_event_cb(lv_event_t * e);
    static void calibrate_event_cb(lv_event_t * e);
    static void programming_setup_event_cb(lv_event_t * e);
    static void screenshot_event_cb(lv_event_t * e);
    // static void demo_event_cb(lv_event_t * e);
    static void throttle_programming_event_cb(lv_event_t * e);

  public:
    static bool throttleProgrammingActive;

    SettingsUI(DCCEXProtocol& dccex, lv_obj_t* parent);
    ~SettingsUI() override;

    class ProgramUI* getProgramUI() { return _programUI; }
};
