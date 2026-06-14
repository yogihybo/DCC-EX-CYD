#pragma once

#include <LVGL_CYD.h>
#include <DCCExCS.h>
#include <Locos.h>
#include <ArduinoJson.h>
#include <bitset>
#include <vector>
#include "LVGL_Layouts.h"

class LocoUI : public UIView {
  private:
    DCCExCS& _dccExCS;
    Locos& _locos;
    DCCExCS::Loco _loco;
    uint8_t _broadcastLocoHandler;

    lv_obj_t* _container;
    lv_obj_t* _selectionMenu;
    lv_obj_t* _nameMenu;
    lv_obj_t* _addressLabel;
    lv_obj_t* _nameLabel;
    
    lv_obj_t* _speedArc;
    lv_obj_t* _speedLabel;
    lv_obj_t* _dirBtn;
    lv_obj_t* _dirLabel;
    lv_obj_t* _keyboard;
    lv_obj_t* _textarea;

    std::vector<lv_obj_t*> _fnButtons;
    uint8_t _fnPage = 0; 

    StaticJsonDocument<10240> _locoDoc;
    JsonArrayConst _locoFunctions;

    void buildSelectionMenu();
    void buildControlScreen();
    void buildFunctionButtons();
    void renderFunctionPage();
    void toggleFunctionButtons(std::bitset<32> toggle);

    void showKeypad();
    void hideKeypad();
    void refresh();

    void broadcast(void* parameter);

    static void addr_btn_event_cb(lv_event_t * e);
    static void kb_event_cb(lv_event_t * e);
    
    static void open_selection_event_cb(lv_event_t * e);
    static void close_selection_event_cb(lv_event_t * e);

    static void name_btn_event_cb(lv_event_t * e);
    static void close_name_menu_event_cb(lv_event_t * e);
    static void loco_selected_event_cb(lv_event_t * e);

    static void group_btn_event_cb(lv_event_t * e);
    static void group_selected_event_cb(lv_event_t * e);

    static void nav_btn_event_cb(lv_event_t * e);
    static void dir_btn_event_cb(lv_event_t * e);
    static void speed_arc_event_cb(lv_event_t * e);
    static void fn_btn_event_cb(lv_event_t * e);
    static void page_btn_event_cb(lv_event_t * e);
    static void estop_btn_event_cb(lv_event_t * e);

  public:
    LocoUI(DCCExCS& dccExCS, Locos& locos, lv_obj_t* parent);
    ~LocoUI() override;
};
