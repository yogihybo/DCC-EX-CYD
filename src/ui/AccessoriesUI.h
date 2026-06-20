#pragma once

#include <LVGL_CYD.h>
#include <DCCEXProtocol.h>
#include "LVGL_Layouts.h"

class AccessoriesUI : public UIView {
  private:
    DCCEXProtocol& _dccex;

    lv_obj_t* _container;
    lv_obj_t* _content;
    lv_obj_t* _keyboard;
    lv_obj_t* _textarea;
    lv_obj_t* _recents_cont;
    lv_obj_t* _btn_throw;
    lv_obj_t* _btn_close;
    lv_obj_t* _status_lbl;

    int      _lastState;  // -1 = unknown, 0 = closed, 1 = thrown
    uint16_t _lastAddr;

    static const int MAX_RECENTS = 5;
    uint16_t _recents[MAX_RECENTS];
    int      _recentCount;

    void _buildContent();
    void sendCommand(bool thrown);
    void addRecent(uint16_t addr);
    void rebuildRecents();
    void updateStatus(bool thrown, uint16_t addr);
    void updateButtonStyles();

    static void textarea_event_cb(lv_event_t* e);
    static void kb_event_cb(lv_event_t* e);
    static void action_event_cb(lv_event_t* e);
    static void chip_event_cb(lv_event_t* e);

  public:
    AccessoriesUI(DCCEXProtocol& dccex, lv_obj_t* parent);
    ~AccessoriesUI() override;
};
