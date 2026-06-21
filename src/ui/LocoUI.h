#pragma once

#include <LVGL_CYD.h>
#include <DCCEXProtocol.h>
#include <Locos.h>
#include <ArduinoJson.h>
#include <bitset>
#include <memory>
#include <vector>
#include "LVGL_Layouts.h"
#include "ConsistUI.h"

class LocoUI : public UIView {
  private:
    DCCEXProtocol& _dccex;
    Locos& _locos;

    struct LocoState {
        uint16_t address = 0;
        int speed = 0;
        bool direction = true; // true = FWD
        std::bitset<32> functions;
    } _loco;

    Loco* _activeLoco = nullptr;
    CSConsist* _activeConsist = nullptr;
    String _consistName;
    uint32_t _lastLocalSpeedMs = 0;
    static constexpr uint32_t SPEED_LOCAL_HOLD_MS = 400;

    lv_obj_t* _container;
    lv_obj_t* _selectionMenu;
    lv_obj_t* _nameMenu;
    lv_obj_t* _addressLabel;
    lv_obj_t* _nameLabel;

    String _locoName;

    lv_obj_t* _prevBtn;
    lv_obj_t* _nextBtn;
    lv_obj_t* _speedArc;
    lv_obj_t* _speedLabel;
    lv_obj_t* _dirBtn;      // lv_switch
    lv_obj_t* _dirFwdLabel;
    lv_obj_t* _dirRevLabel;
    lv_obj_t* _pageBtn;
    lv_obj_t* _pageBtnLabel;
    lv_obj_t* _lockOverlay;
    lv_obj_t* _keyboard;
    lv_obj_t* _textarea;

    std::vector<lv_obj_t*> _fnButtons;
    std::unique_ptr<DynamicJsonDocument> _groupsDoc;
    uint8_t _fnPage = 0;

    void buildSelectionMenu();
    void buildControlScreen();
    void buildFunctionButtons(JsonDocument& locoDoc);
    void renderFunctionPage();
    void toggleFunctionButtons(std::bitset<32> toggle);

    void showKeypad();
    void hideKeypad();
    void refresh();

    static void addr_btn_event_cb(lv_event_t * e);
    static void kb_event_cb(lv_event_t * e);

    static void open_selection_event_cb(lv_event_t * e);
    static void close_selection_event_cb(lv_event_t * e);

    static void name_btn_event_cb(lv_event_t * e);
    static void close_name_menu_event_cb(lv_event_t * e);
    static void loco_selected_event_cb(lv_event_t * e);

    static void group_btn_event_cb(lv_event_t * e);
    static void group_selected_event_cb(lv_event_t * e);

    static void consist_btn_event_cb(lv_event_t * e);

    static void release_btn_event_cb(lv_event_t * e);

    static void nav_btn_event_cb(lv_event_t * e);
    static void dir_btn_event_cb(lv_event_t * e);
    static void speed_arc_event_cb(lv_event_t * e);
    static void fn_btn_event_cb(lv_event_t * e);
    static void page_btn_event_cb(lv_event_t * e);
    static void estop_btn_event_cb(lv_event_t * e);

  public:
    LocoUI(DCCEXProtocol& dccex, Locos& locos, lv_obj_t* parent);
    ~LocoUI() override;

    void nudgeSpeed(int delta);
    void onLocoUpdate(Loco* loco);
    void onConsistUpdate(int leadLoco, CSConsist* consist);
    void demoStep(int step);
};
