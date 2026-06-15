#include "LocoUI.h"
#include <FileSystems.h>
#include <SD.h>
#include <StreamUtils.h>
#include <Settings.h>

LocoUI::LocoUI(DCCExCS& dccExCS, Locos& locos, lv_obj_t* parent) 
    : _dccExCS(dccExCS), _locos(locos), _loco((uint16_t)locos), _broadcastLocoHandler(0xFF), _fnPage(0) {
    
    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(_container, 0, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);

    _keyboard = nullptr;
    _textarea = nullptr;
    _nameMenu = nullptr;

    if (_loco.address == 0) {
        buildControlScreen();
        buildSelectionMenu();
        // lv_obj_clear_flag(_selectionMenu, LV_OBJ_FLAG_HIDDEN); // Keep hidden so throttle page is default
    } else {
        _broadcastLocoHandler = _dccExCS.addEventListener(DCCExCS::Event::BROADCAST_LOCO, [this](void* parameter) {
            if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
                broadcast(parameter);
                xSemaphoreGive(lvgl_mutex);
            }
        });
        buildControlScreen();
        buildSelectionMenu();
        _dccExCS.acquireLoco(_loco.address);
    }
}

LocoUI::~LocoUI() {
    if (_broadcastLocoHandler != 0xFF) {
        _dccExCS.removeEventListener(DCCExCS::Event::BROADCAST_LOCO, _broadcastLocoHandler);
    }
    if (_keyboard) lv_obj_del(_keyboard);
    if (_textarea) lv_obj_del(_textarea);
    if (_container) lv_obj_del(_container);
}

void LocoUI::broadcast(void* parameter) {
    auto broadcast = *static_cast<DCCExCS::Loco*>(parameter);
    if (_loco.address == broadcast.address) {
        if (_loco.speed != broadcast.speed) {
            lv_arc_set_value(_speedArc, broadcast.speed);
            lv_label_set_text_fmt(_speedLabel, "%d\nkm/h", broadcast.speed);
            
            lv_color_t color;
            if (broadcast.speed < 42) color = lv_color_make(50, 255, 50);
            else if (broadcast.speed < 84) color = lv_color_make(255, 255, 50);
            else color = lv_color_make(255, 50, 50);
            lv_obj_set_style_arc_color(_speedArc, color, LV_PART_INDICATOR);
        }
        if (_loco.direction != broadcast.direction) {
            lv_label_set_text(_dirLabel, broadcast.direction ? "FWD" : "REV");
            lv_obj_set_style_bg_color(_dirBtn, broadcast.direction ? lv_color_make(50, 200, 50) : lv_color_make(200, 200, 50), 0);
        }
        if (_loco.functions != broadcast.functions) {
            toggleFunctionButtons(_loco.functions ^ broadcast.functions);
        }
        _loco = broadcast;
    }
}

void LocoUI::buildSelectionMenu() {
    _selectionMenu = lv_obj_create(_container);
    lv_obj_set_size(_selectionMenu, LV_PCT(90), LV_PCT(90));
    lv_obj_align(_selectionMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(_selectionMenu, 0, 0);
    lv_obj_set_style_border_width(_selectionMenu, 0, 0);
    lv_obj_set_style_radius(_selectionMenu, 12, 0);
    lv_obj_set_style_shadow_width(_selectionMenu, 30, 0);
    lv_obj_set_style_shadow_opa(_selectionMenu, LV_OPA_50, 0);
    lv_obj_add_flag(_selectionMenu, LV_OBJ_FLAG_HIDDEN); // Hidden by default

    // Title Row
    lv_obj_t* title_row = lv_obj_create(_selectionMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 40);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select Locomotive");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* close_btn = lv_btn_create(title_row);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_make(200, 50, 50), 0);
    lv_obj_t* close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "Back");
    lv_obj_center(close_lbl);
    lv_obj_add_event_cb(close_btn, close_selection_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_addr = lv_btn_create(_selectionMenu);
    lv_obj_set_size(btn_addr, 200, 35);
    lv_obj_align(btn_addr, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_t* lbl_addr = lv_label_create(btn_addr);
    lv_label_set_text(lbl_addr, "By Address");
    lv_obj_center(lbl_addr);
    lv_obj_add_event_cb(btn_addr, addr_btn_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_name = lv_btn_create(_selectionMenu);
    lv_obj_set_size(btn_name, 200, 35);
    lv_obj_align(btn_name, LV_ALIGN_TOP_MID, 0, 85);
    lv_obj_t* lbl_name = lv_label_create(btn_name);
    lv_label_set_text(lbl_name, "By Name");
    lv_obj_center(lbl_name);
    lv_obj_add_event_cb(btn_name, name_btn_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_group = lv_btn_create(_selectionMenu);
    lv_obj_set_size(btn_group, 200, 35);
    lv_obj_align(btn_group, LV_ALIGN_TOP_MID, 0, 125);
    lv_obj_t* lbl_group = lv_label_create(btn_group);
    lv_label_set_text(lbl_group, "By Group");
    lv_obj_center(lbl_group);
    lv_obj_add_event_cb(btn_group, group_btn_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* btn_release = lv_btn_create(_selectionMenu);
    lv_obj_set_size(btn_release, 200, 35);
    lv_obj_align(btn_release, LV_ALIGN_TOP_MID, 0, 165);
    lv_obj_set_style_bg_color(btn_release, lv_color_make(200, 50, 50), 0);
    lv_obj_t* lbl_release = lv_label_create(btn_release);
    lv_label_set_text(lbl_release, "Release");
    lv_obj_center(lbl_release);
    lv_obj_add_event_cb(btn_release, release_btn_event_cb, LV_EVENT_CLICKED, this);
}

void LocoUI::buildControlScreen() {
    DynamicJsonDocument locoDoc(10240);

    char path[32];
    sprintf(path, "/locos/%d.json", _loco.address);
    
    fs::FS& fs = Settings.getFS();

    if (_loco.address != 0 && fs.exists(path)) {
        File locoFile = fs.open(path);
        deserializeJson(locoDoc, locoFile);
        locoFile.close();
    }

    String nameStr = "Unknown Loco";
    if (locoDoc.containsKey("name")) nameStr = locoDoc["name"].as<const char*>();

    // 1. Top Section (Name & Address Arrows)
    _nameLabel = lv_label_create(_container);
    lv_label_set_text(_nameLabel, nameStr.c_str());
    lv_obj_set_style_text_color(_nameLabel, lv_color_make(100, 100, 255), 0);
    lv_obj_align(_nameLabel, LV_ALIGN_TOP_MID, 0, 2);

    lv_obj_t* prev_btn = lv_btn_create(_container);
    lv_obj_set_size(prev_btn, 40, 40);
    lv_obj_align(prev_btn, LV_ALIGN_TOP_LEFT, 0, 15);
    lv_obj_set_style_bg_opa(prev_btn, 0, 0);
    lv_obj_set_style_shadow_width(prev_btn, 0, 0);
    lv_obj_t* pl = lv_label_create(prev_btn);
    lv_label_set_text(pl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(pl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(pl, lv_color_make(180, 180, 180), 0);
    lv_obj_center(pl);
    lv_obj_add_event_cb(prev_btn, nav_btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(prev_btn, (void*)0);

    lv_obj_t* addr_btn = lv_btn_create(_container);
    lv_obj_set_size(addr_btn, 80, 40);
    lv_obj_align(addr_btn, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_bg_opa(addr_btn, 0, 0); // Transparent
    lv_obj_set_style_shadow_width(addr_btn, 0, 0);
    lv_obj_add_event_cb(addr_btn, open_selection_event_cb, LV_EVENT_CLICKED, this);

    _addressLabel = lv_label_create(addr_btn);
    if (_loco.address > 0) {
        lv_label_set_text_fmt(_addressLabel, "%d", _loco.address);
    } else {
        lv_label_set_text(_addressLabel, "None");
    }
    lv_obj_set_style_text_font(_addressLabel, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(_addressLabel, lv_color_make(100, 100, 255), 0);
    lv_obj_center(_addressLabel);

    lv_obj_t* next_btn = lv_btn_create(_container);
    lv_obj_set_size(next_btn, 40, 40);
    lv_obj_align(next_btn, LV_ALIGN_TOP_RIGHT, 0, 15);
    lv_obj_set_style_bg_opa(next_btn, 0, 0);
    lv_obj_set_style_shadow_width(next_btn, 0, 0);
    lv_obj_t* nl = lv_label_create(next_btn);
    lv_label_set_text(nl, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(nl, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(nl, lv_color_make(180, 180, 180), 0);
    lv_obj_center(nl);
    lv_obj_add_event_cb(next_btn, nav_btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(next_btn, (void*)1);

    // 2. Central Speedometer (lv_arc)
    _speedArc = lv_arc_create(_container);
    lv_obj_set_size(_speedArc, 130, 130);
    lv_arc_set_rotation(_speedArc, 135);
    lv_arc_set_bg_angles(_speedArc, 0, 270);
    lv_arc_set_range(_speedArc, 0, 126);
    lv_arc_set_value(_speedArc, _loco.speed);
    lv_color_t color;
    if (_loco.speed < 42) color = lv_color_make(50, 255, 50);
    else if (_loco.speed < 84) color = lv_color_make(255, 255, 50);
    else color = lv_color_make(255, 50, 50);
    lv_obj_set_style_arc_color(_speedArc, color, LV_PART_INDICATOR);
    
    lv_obj_align(_speedArc, LV_ALIGN_CENTER, 0, -5);
    lv_obj_add_event_cb(_speedArc, speed_arc_event_cb, LV_EVENT_VALUE_CHANGED, this);

    _speedLabel = lv_label_create(_speedArc);
    lv_label_set_text_fmt(_speedLabel, "%d\nkm/h", _loco.speed);
    lv_obj_set_style_text_align(_speedLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(_speedLabel, &lv_font_montserrat_24, 0);
    lv_obj_center(_speedLabel);

    // 3. Bottom Tool Buttons (E-Stop, FWD/REV, F>>)
    lv_obj_t* estop_btn = lv_btn_create(_container);
    lv_obj_set_size(estop_btn, 50, 35);
    lv_obj_align(estop_btn, LV_ALIGN_BOTTOM_LEFT, 2, -2);
    lv_obj_set_style_bg_color(estop_btn, lv_color_make(255, 50, 50), 0);
    lv_obj_t* el = lv_label_create(estop_btn);
    lv_label_set_text(el, "STOP");
    lv_obj_center(el);
    lv_obj_add_event_cb(estop_btn, estop_btn_event_cb, LV_EVENT_CLICKED, this);

    _dirBtn = lv_btn_create(_container);
    lv_obj_set_size(_dirBtn, 60, 35);
    lv_obj_align(_dirBtn, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(_dirBtn, _loco.direction ? lv_color_make(50, 200, 50) : lv_color_make(200, 200, 50), 0);
    _dirLabel = lv_label_create(_dirBtn);
    lv_label_set_text(_dirLabel, _loco.direction ? "FWD" : "REV");
    lv_obj_center(_dirLabel);
    lv_obj_add_event_cb(_dirBtn, dir_btn_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* page_btn = lv_btn_create(_container);
    lv_obj_set_size(page_btn, 50, 35);
    lv_obj_align(page_btn, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
    lv_obj_set_style_bg_color(page_btn, lv_color_make(50, 50, 50), 0);
    lv_obj_t* pl2 = lv_label_create(page_btn);
    lv_label_set_text(pl2, "F>>");
    lv_obj_center(pl2);
    lv_obj_add_event_cb(page_btn, page_btn_event_cb, LV_EVENT_CLICKED, this);

    // 4. Build Function Buttons
    buildFunctionButtons(locoDoc);
    renderFunctionPage();
}

void LocoUI::buildFunctionButtons(JsonDocument& locoDoc) {
    JsonArrayConst locoFunctions;
    if (locoDoc["functions"].is<JsonArray>()) {
        locoFunctions = locoDoc["functions"].as<JsonArray>();
    } else {
        File functionFile;
        fs::FS& fs = Settings.getFS();
        
        if (locoDoc.containsKey("functions")) {
            const char* fnPath = locoDoc["functions"].as<const char*>();
            if (fs.exists(fnPath)) functionFile = fs.open(fnPath);
        }
        if (!functionFile) {
            if (WebsiteFS.exists("/default.json")) {
                functionFile = WebsiteFS.open("/default.json");
            } else if (fs.exists("/default.json")) {
                functionFile = fs.open("/default.json");
            }
        }

        if (functionFile) {
            StaticJsonDocument<16> filter;
            filter["functions"] = true;
            deserializeJson(locoDoc, functionFile, DeserializationOption::Filter(filter));
            locoFunctions = locoDoc["functions"].as<JsonArray>();
            functionFile.close();
        }
    }

    for (JsonArrayConst const& row : locoFunctions) {
        for (JsonObjectConst const& fn : row) {
            uint8_t func = fn["fn"];
            bool latching = fn["latching"] | true;
            const char* idle_label = fn["btn"]["idle"]["label"].as<const char*>();
            const char* idle_icon = fn["btn"]["idle"]["icon"].as<const char*>();
            const char* pressed_label = fn["btn"]["pressed"]["label"].as<const char*>();
            const char* pressed_icon = fn["btn"]["pressed"]["icon"].as<const char*>();
            
            lv_obj_t* btn = lv_btn_create(_container);
            lv_obj_set_size(btn, 42, 30);
            
            if (latching) lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
            
            auto create_visual = [btn, func](const char* icon, const char* label) -> lv_obj_t* {
                lv_obj_t* visual_obj = nullptr;
                if (icon && strlen(icon) > 0) {
                    String fsPath = icon;
                    if (fsPath.startsWith("/$/")) fsPath = fsPath.substring(2);
                    if (!fsPath.startsWith("/")) fsPath = "/icons/" + fsPath;
                    if (!fsPath.endsWith(".bmp")) fsPath += ".bmp";
                    
                    String fullIconPath = "";
                    if (WebsiteFS.exists(fsPath)) fullIconPath = "W:" + fsPath;
                    else if (Settings.getFS().exists(fsPath)) fullIconPath = "S:" + fsPath;
                    
                    if (fullIconPath.length() > 0) {
                        visual_obj = lv_image_create(btn);
                        char* pathBuf = (char*)malloc(fullIconPath.length() + 1);
                        strcpy(pathBuf, fullIconPath.c_str());
                        lv_image_set_src(visual_obj, pathBuf);
                        lv_obj_center(visual_obj);
                        lv_obj_add_event_cb(visual_obj, [](lv_event_t* e) {
                            char* buf = (char*)lv_event_get_user_data(e);
                            if (buf) free(buf);
                        }, LV_EVENT_DELETE, pathBuf);
                        return visual_obj;
                    }
                }
                
                visual_obj = lv_label_create(btn);
                if (label && strlen(label) > 0) lv_label_set_text(visual_obj, label);
                else lv_label_set_text_fmt(visual_obj, "F%d", func);
                lv_obj_center(visual_obj);
                return visual_obj;
            };

            lv_obj_t* idle_obj = create_visual(idle_icon, idle_label);
            lv_obj_t* pressed_obj = nullptr;

            if ((pressed_label && strlen(pressed_label) > 0) || (pressed_icon && strlen(pressed_icon) > 0)) {
                pressed_obj = create_visual(pressed_icon, pressed_label);
            } else {
                pressed_obj = create_visual(idle_icon, idle_label);
            }

            bool is_checked = _loco.functions.test(func);
            if (is_checked) {
                lv_obj_add_state(btn, LV_STATE_CHECKED);
                lv_obj_add_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
            }

            lv_obj_set_user_data(btn, (void*)(uintptr_t)func);
            lv_obj_add_event_cb(btn, fn_btn_event_cb, LV_EVENT_VALUE_CHANGED, this);
            lv_obj_add_event_cb(btn, fn_btn_event_cb, LV_EVENT_CLICKED, this);
            
            lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN); // Hidden initially
            _fnButtons.push_back(btn);
        }
    }
}

void LocoUI::renderFunctionPage() {
    for (auto btn : _fnButtons) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }

    int start_idx = _fnPage * 10;
    for (int i = 0; i < 10; i++) {
        int idx = start_idx + i;
        if (idx >= _fnButtons.size()) break;

        lv_obj_t* btn = _fnButtons[idx];
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);

        // Calculate position
        // Left column (i < 5), Right column (i >= 5)
        int y_pos = 50 + (i % 5) * 32;
        if (i < 5) {
            lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 2, y_pos);
        } else {
            lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -2, y_pos);
        }
    }
}

void LocoUI::toggleFunctionButtons(std::bitset<32> toggle) {
    for (auto child : _fnButtons) {
        uint8_t func = (uintptr_t)lv_obj_get_user_data(child);
        if (toggle.test(func)) {
            bool is_checked = _loco.functions.test(func);
            if (is_checked) {
                lv_obj_add_state(child, LV_STATE_CHECKED);
            } else {
                lv_obj_clear_state(child, LV_STATE_CHECKED);
            }
            if (lv_obj_get_child_cnt(child) >= 2) {
                lv_obj_t* idle_obj = lv_obj_get_child(child, 0);
                lv_obj_t* pressed_obj = lv_obj_get_child(child, 1);
                if (is_checked) {
                    lv_obj_add_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }
}

void LocoUI::showKeypad() {
  if (!_keyboard) {
    _keyboard = lv_obj_create(_container);
    lv_obj_set_size(_keyboard, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_keyboard, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(_keyboard, 0, 0);
    lv_obj_set_style_border_width(_keyboard, 0, 0);
    lv_obj_set_flex_flow(_keyboard, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(_keyboard);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 40);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Address");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* close_btn = lv_btn_create(title_row);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_make(200, 50, 50), 0);
    lv_obj_t* close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "Back");
    lv_obj_center(close_lbl);

    _textarea = lv_textarea_create(_keyboard);
    lv_obj_set_width(_textarea, LV_PCT(100));
    lv_textarea_set_one_line(_textarea, true);
    lv_textarea_set_placeholder_text(_textarea, "Loco Address");

    lv_obj_t* kb = lv_keyboard_create(_keyboard);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, _textarea);
    lv_obj_set_flex_grow(kb, 1);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, this);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, this);

    lv_obj_add_event_cb(close_btn, [](lv_event_t* e) {
        lv_obj_t* kb = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_send_event(kb, LV_EVENT_CANCEL, NULL);
    }, LV_EVENT_CLICKED, kb);
  }
}

void LocoUI::hideKeypad() {
  if (_keyboard) { lv_obj_delete_async(_keyboard); _keyboard = nullptr; }
  if (_textarea) { lv_obj_delete_async(_textarea); _textarea = nullptr; }
}

void LocoUI::refresh() {
    lv_obj_clean(_container);
    _fnButtons.clear();

    uint16_t newAddr = (uint16_t)_locos;
    if (_loco.address != newAddr) {
        _loco.address = newAddr;
        _loco.speed = 0;
        _loco.direction = 1; // FWD
        _loco.functions.reset();
    }
    
    if (_loco.address == 0) {
        buildControlScreen();
        buildSelectionMenu();
        // lv_obj_clear_flag(_selectionMenu, LV_OBJ_FLAG_HIDDEN); // Don't pop up automatically
    } else {
        if (_broadcastLocoHandler == 0xFF) {
            _broadcastLocoHandler = _dccExCS.addEventListener(DCCExCS::Event::BROADCAST_LOCO, [this](void* parameter) {
                if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
                    broadcast(parameter);
                    xSemaphoreGive(lvgl_mutex);
                }
            });
        }
        buildControlScreen();
        buildSelectionMenu();
        _dccExCS.acquireLoco(_loco.address);
    }
}

void LocoUI::open_selection_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_move_foreground(ui->_selectionMenu);
    lv_obj_clear_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
}

void LocoUI::close_selection_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) {
        lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
    }
}

void LocoUI::name_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    if (ui->_nameMenu) {
        lv_obj_delete_async(ui->_nameMenu);
        ui->_nameMenu = nullptr;
    }

    ui->_nameMenu = lv_obj_create(ui->_container);
    lv_obj_set_size(ui->_nameMenu, LV_PCT(90), LV_PCT(90));
    lv_obj_align(ui->_nameMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->_nameMenu, 0, 0);
    lv_obj_set_style_border_width(ui->_nameMenu, 0, 0);
    lv_obj_set_style_radius(ui->_nameMenu, 12, 0);
    lv_obj_set_style_shadow_width(ui->_nameMenu, 30, 0);
    lv_obj_set_style_shadow_opa(ui->_nameMenu, LV_OPA_50, 0);
    lv_obj_set_flex_flow(ui->_nameMenu, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 40);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Name");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* close_btn = lv_btn_create(title_row);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_make(200, 50, 50), 0);
    lv_obj_t* close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "Back");
    lv_obj_center(close_lbl);
    lv_obj_add_event_cb(close_btn, close_name_menu_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t* list = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 5, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    auto addLocos = [ui, list](fs::FS& fs) {
        File dir = fs.open("/locos");
        if (dir) {
            while (File file = dir.openNextFile()) {
                if (!file.isDirectory()) {
                    uint16_t address = strtoul(file.name(), (char**)NULL, 10);
                    
                    StaticJsonDocument<16> filterDoc;
                    filterDoc["name"] = true;
                    StaticJsonDocument<256> locoDoc;
                    deserializeJson(locoDoc, file, DeserializationOption::Filter(filterDoc));
                    
                    String nameStr = "#";
                    nameStr += address;
                    nameStr += " - ";
                    if (locoDoc.containsKey("name")) nameStr += locoDoc["name"].as<const char*>();
                    else nameStr += "Unknown";

                    lv_obj_t* btn = lv_btn_create(list);
                    lv_obj_set_width(btn, LV_PCT(100));
                    lv_obj_t* lbl = lv_label_create(btn);
                    lv_label_set_text(lbl, nameStr.c_str());
                    lv_obj_center(lbl);

                    lv_obj_set_user_data(btn, (void*)(uintptr_t)address);
                    lv_obj_add_event_cb(btn, loco_selected_event_cb, LV_EVENT_CLICKED, ui);
                }
                file.close();
            }
            dir.close();
        }
    };

    addLocos(Settings.getFS());
}

void LocoUI::loco_selected_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    uint16_t address = (uintptr_t)lv_obj_get_user_data(btn);
    
    if (address > 0 && address <= 9999) {
        ui->_locos.add(address);
        ui->_nameMenu = nullptr; 
        lv_async_call([](void* user_data) {
            ((LocoUI*)user_data)->refresh();
        }, ui);
    }
}

void LocoUI::close_name_menu_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_nameMenu) {
        lv_obj_delete_async(ui->_nameMenu);
        ui->_nameMenu = nullptr;
    }
    if (ui->_selectionMenu) {
        lv_obj_clear_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
    }
}

void LocoUI::group_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    if (ui->_nameMenu) {
        lv_obj_delete_async(ui->_nameMenu);
        ui->_nameMenu = nullptr;
    }

    ui->_nameMenu = lv_obj_create(ui->_container);
    lv_obj_set_size(ui->_nameMenu, LV_PCT(90), LV_PCT(90));
    lv_obj_align(ui->_nameMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(ui->_nameMenu, 0, 0);
    lv_obj_set_style_border_width(ui->_nameMenu, 0, 0);
    lv_obj_set_style_radius(ui->_nameMenu, 12, 0);
    lv_obj_set_style_shadow_width(ui->_nameMenu, 30, 0);
    lv_obj_set_style_shadow_opa(ui->_nameMenu, LV_OPA_50, 0);
    lv_obj_set_flex_flow(ui->_nameMenu, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 40);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Group");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t* close_btn = lv_btn_create(title_row);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_make(200, 50, 50), 0);
    lv_obj_t* close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "Back");
    lv_obj_center(close_lbl);
    lv_obj_add_event_cb(close_btn, close_name_menu_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t* list = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 5, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    fs::FS& fs = Settings.getFS();

    if (fs.exists("/groups.json")) {
        File groupsFile = fs.open("/groups.json");
        DynamicJsonDocument groupsDoc(4096);
        deserializeJson(groupsDoc, groupsFile);
        groupsFile.close();

        JsonArray groups = groupsDoc.as<JsonArray>();
        int index = 0;
        for (JsonObjectConst group : groups) {
            lv_obj_t* btn = lv_btn_create(list);
            lv_obj_set_width(btn, LV_PCT(100));
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, group["name"] | "Unnamed Group");
            lv_obj_center(lbl);

            lv_obj_set_user_data(btn, (void*)(uintptr_t)index);
            lv_obj_add_event_cb(btn, group_selected_event_cb, LV_EVENT_CLICKED, ui);
            index++;
        }
    } else {
        lv_obj_t* lbl = lv_label_create(list);
        lv_label_set_text(lbl, "No groups found");
        lv_obj_center(lbl);
    }
}

void LocoUI::group_selected_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int index = (uintptr_t)lv_obj_get_user_data(btn);
    
    fs::FS& fs = Settings.getFS();
    if (!fs.exists("/groups.json")) return;
    
    File groupsFile = fs.open("/groups.json");
    DynamicJsonDocument groupsDoc(4096);
    deserializeJson(groupsDoc, groupsFile);
    groupsFile.close();

    JsonArray groups = groupsDoc.as<JsonArray>();
    if (index >= 0 && index < groups.size()) {
        JsonObjectConst group = groups[index];
        JsonArrayConst locos = group["locos"];
        
        // Clear the current list in _nameMenu
        lv_obj_t* list = lv_obj_get_child(ui->_nameMenu, 1); // 0 is title_row, 1 is list
        lv_obj_clean(list);

        // Change title
        lv_obj_t* title_row = lv_obj_get_child(ui->_nameMenu, 0);
        lv_obj_t* title = lv_obj_get_child(title_row, 0);
        lv_label_set_text_fmt(title, "Group: %s", group["name"] | "Unknown");

        fs::FS& fs = Settings.getFS();

        for (uint16_t address : locos) {
            char path[32];
            sprintf(path, "/locos/%d.json", address);
            
            String nameStr = "#";
            nameStr += address;
            nameStr += " - ";

            if (fs.exists(path)) {
                StaticJsonDocument<16> filterDoc;
                filterDoc["name"] = true;
                StaticJsonDocument<256> locoDoc;
                File locoFile = fs.open(path);
                deserializeJson(locoDoc, locoFile, DeserializationOption::Filter(filterDoc));
                locoFile.close();
                if (locoDoc.containsKey("name")) nameStr += locoDoc["name"].as<const char*>();
                else nameStr += "Unknown";
            } else {
                nameStr += "Missing";
            }

            lv_obj_t* loco_btn = lv_btn_create(list);
            lv_obj_set_width(loco_btn, LV_PCT(100));
            lv_obj_t* lbl = lv_label_create(loco_btn);
            lv_label_set_text(lbl, nameStr.c_str());
            lv_obj_center(lbl);

            lv_obj_set_user_data(loco_btn, (void*)(uintptr_t)address);
            lv_obj_add_event_cb(loco_btn, loco_selected_event_cb, LV_EVENT_CLICKED, ui);
        }
    }
}

void LocoUI::release_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_loco.address != 0) {
        ui->_dccExCS.setLocoThrottle(ui->_loco.address, 0, ui->_loco.direction);
        ui->_dccExCS.releaseLoco(ui->_loco.address);
        ui->_locos.remove();
        
        if (ui->_selectionMenu) {
            lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
        }

        lv_async_call([](void* user_data) {
            ((LocoUI*)user_data)->refresh();
        }, ui);
    }
}

void LocoUI::addr_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
    ui->showKeypad();
}

void LocoUI::kb_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        const char* txt = lv_textarea_get_text(ui->_textarea);
        if (txt && strlen(txt) > 0) {
            uint16_t addr = atoi(txt);
            if (addr > 0 && addr <= 9999) {
                ui->_locos.add(addr);
                ui->_keyboard = nullptr;
                ui->_textarea = nullptr;
                lv_async_call([](void* user_data) {
                    ((LocoUI*)user_data)->refresh();
                }, ui);
                return;
            }
        }
    }
    ui->hideKeypad();
}

void LocoUI::nav_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    intptr_t action = (intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
    if (action == 0) ui->_locos.prev();
    else ui->_locos.next();
    ui->refresh();
}

void LocoUI::dir_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    ui->_loco.direction = !ui->_loco.direction;
    if (ui->_dirLabel) {
        lv_label_set_text(ui->_dirLabel, ui->_loco.direction ? "FWD" : "REV");
        lv_obj_set_style_bg_color(ui->_dirBtn, ui->_loco.direction ? lv_color_make(50, 200, 50) : lv_color_make(200, 200, 50), 0);
    }
    ui->_dccExCS.setLocoThrottle(ui->_loco.address, ui->_loco.speed, ui->_loco.direction);
}

void LocoUI::speed_arc_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* arc = (lv_obj_t*)lv_event_get_target(e);
    int speed = lv_arc_get_value(arc);
    
    if (ui->_speedLabel) {
        lv_label_set_text_fmt(ui->_speedLabel, "%d\nkm/h", speed);
    }
    
    lv_color_t color;
    if (speed < 42) color = lv_color_make(50, 255, 50);
    else if (speed < 84) color = lv_color_make(255, 255, 50);
    else color = lv_color_make(255, 50, 50);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    
    ui->_dccExCS.setLocoThrottle(ui->_loco.address, speed, ui->_loco.direction);
}

void LocoUI::fn_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    uint8_t func = (uintptr_t)lv_obj_get_user_data(btn);
    bool is_checked = lv_obj_has_state(btn, LV_STATE_CHECKED);

    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        if (lv_obj_get_child_cnt(btn) >= 2) {
            lv_obj_t* idle_obj = lv_obj_get_child(btn, 0);
            lv_obj_t* pressed_obj = lv_obj_get_child(btn, 1);
            if (is_checked) {
                lv_obj_add_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    ui->_dccExCS.setLocoFn(ui->_loco.address, func, is_checked);
}

void LocoUI::page_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    ui->_fnPage++;
    if (ui->_fnPage * 10 >= ui->_fnButtons.size()) ui->_fnPage = 0;
    ui->renderFunctionPage();
}

void LocoUI::estop_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    ui->_dccExCS.setLocoThrottle(ui->_loco.address, 0, ui->_loco.direction);
}
