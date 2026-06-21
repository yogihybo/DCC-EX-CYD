#pragma once

#include <LVGL_CYD.h>
#include <DCCEXProtocol.h>
#include "LVGL_Layouts.h"

class PowerUI : public UIView {
  private:
    DCCEXProtocol& _dccex;
    bool _updatingFromBroadcast = false;
    bool _mainOn = false;
    bool _progOn = false;
    bool _joinOn = false;

    lv_obj_t* _container;
    lv_obj_t* _dot_main;
    lv_obj_t* _dot_prog;
    lv_obj_t* _lbl_main_status;
    lv_obj_t* _lbl_prog_status;
    lv_obj_t* _btn_main_on;
    lv_obj_t* _btn_main_off;
    lv_obj_t* _btn_prog_on;
    lv_obj_t* _btn_prog_off;
    lv_obj_t* _btn_all_on;
    lv_obj_t* _btn_all_off;
    lv_obj_t* _btn_join;

    void updateStyles();
    static void btn_event_cb(lv_event_t* e);

  public:
    PowerUI(DCCEXProtocol& dccex, lv_obj_t* parent);
    ~PowerUI() override;

    void onPowerUpdate(bool main, bool prog, bool join);
    void onIndividualPowerUpdate(TrackPower state, int track);
    // void demoStep(int step);
};
