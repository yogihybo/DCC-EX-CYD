#pragma once

#include <LVGL_CYD.h>
#include <DCCExCS.h>
#include "LVGL_Layouts.h"

class AccessoriesUI : public UIView {
  private:
    DCCExCS& _dccExCS;

    lv_obj_t* _container;
    lv_obj_t* _keyboard;
    lv_obj_t* _textarea;
    
    bool _pending_state;

    void showKeypad(bool state);
    void hideKeypad();

    static void btn_event_cb(lv_event_t * e);
    static void kb_event_cb(lv_event_t * e);

  public:
    AccessoriesUI(DCCExCS& dccExCS, lv_obj_t* parent);
    ~AccessoriesUI() override;
};
