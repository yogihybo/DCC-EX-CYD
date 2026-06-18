#pragma once

#include <LVGL_CYD.h>
#include <DCCEXProtocol.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>
#include "LVGL_Layouts.h"

class ConsistUI {
public:
    using DriveCallback = std::function<void(CSConsist*, const String& name)>;

    ConsistUI(DCCEXProtocol& dccex, lv_obj_t* parent, DriveCallback onDrive);
    ~ConsistUI();

private:
    struct MemberData { int address; bool reversed; };

    DCCEXProtocol&          _dccex;
    DriveCallback           _onDrive;
    lv_obj_t*               _container;
    lv_obj_t*               _content = nullptr;

    // Current editing state
    String                  _editName;
    std::vector<MemberData> _members;
    bool                    _replicateFunctions = false;
    int                     _editLeadAddr = -1;   // -1 = new consist
    bool                    _addMemberReversed = false;

    // Add-member keypad widgets (kept for keyboard binding)
    lv_obj_t*               _addrTextarea = nullptr;
    lv_obj_t*               _keyboard = nullptr;

    // State renderers
    void _showList();
    void _showEditor();
    void _showAddMember();
    void _clearContent();
    void _rebuildMemberRows(lv_obj_t* list);

    // Persistence
    static String _consistPath(int leadAddr);
    void _saveConsist();
    void _deleteConsistFile();

    // Event callbacks
    static void close_btn_cb(lv_event_t* e);
    static void new_consist_cb(lv_event_t* e);
    static void consist_item_cb(lv_event_t* e);
    static void consist_drive_cb(lv_event_t* e);
    static void add_member_btn_cb(lv_event_t* e);
    static void remove_member_cb(lv_event_t* e);
    static void toggle_reversed_cb(lv_event_t* e);
    static void replicate_fn_cb(lv_event_t* e);
    static void name_edit_cb(lv_event_t* e);
    static void drive_btn_cb(lv_event_t* e);
    static void save_btn_cb(lv_event_t* e);
    static void delete_btn_cb(lv_event_t* e);
    static void keyboard_cb(lv_event_t* e);
};
