#pragma once

#include <LVGL_CYD.h>
#include <Settings.h>
#include <DCCExCS.h>
#include "LVGL_Layouts.h"

class SettingsUI : public UIView {
  private:
    lv_obj_t* _container;
    DCCExCS& _dccExCS;

    class WiFiUI* _wifiUI;
    class AboutUI* _aboutUI;
    class ProgramUI* _programUI;

    lv_obj_t* _speedStepLbl;
    lv_obj_t* _rotationLbl;
    lv_obj_t* _storageModeLbl;
    lv_obj_t* _pinBtn;
    lv_obj_t* _brightnessLbl;

    static void speed_step_event_cb(lv_event_t * e);
    static void rotation_event_cb(lv_event_t * e);
    static void storage_mode_event_cb(lv_event_t * e);
    static void brightness_btn_event_cb(lv_event_t * e);
    static void brightness_event_cb(lv_event_t * e);
    static void wifi_setup_event_cb(lv_event_t * e);
    static void about_event_cb(lv_event_t * e);
    static void programming_setup_event_cb(lv_event_t * e);

  public:
    SettingsUI(DCCExCS& dccExCS, lv_obj_t* parent);
    ~SettingsUI() override;
};
