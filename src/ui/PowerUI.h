#pragma once

#include <LVGL_CYD.h>
#include <DCCExCS.h>
#include "LVGL_Layouts.h"

class PowerUI : public UIView {
  private:
    DCCExCS& _dccExCS;
    DCCExCS::Power& _power;
    uint8_t _broadcastPowerHandler;

    lv_obj_t* _container;
    lv_obj_t* _powerAll;
    lv_obj_t* _powerMain;
    lv_obj_t* _powerProg;
    lv_obj_t* _powerJoin;

    static void btn_event_cb(lv_event_t * e);

  public:
    PowerUI(DCCExCS& dccExCS, DCCExCS::Power& power, lv_obj_t* parent);
    ~PowerUI() override;
};
