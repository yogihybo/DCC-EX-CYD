#pragma once

#include <LVGL_CYD.h>
#include <DCCExCS.h>
#include "LVGL_Layouts.h"

class AboutUI : public UIView {
  private:
    lv_obj_t* _container;
    DCCExCS& _dccExCS;
    uint16_t _csVersionHandler;

    lv_obj_t* _csVersion;
    lv_obj_t* _csBoard;
    lv_obj_t* _csShield;
    lv_obj_t* _csBuild;

    lv_obj_t* _memLbl;
    lv_obj_t* _wifiStat;
    lv_timer_t* _updateTimer;

    static void update_timer_cb(lv_timer_t* timer);

  public:
    AboutUI(DCCExCS& dccExCS, lv_obj_t* parent);
    ~AboutUI() override;

    lv_obj_t* getContainer() { return _container; }
};
