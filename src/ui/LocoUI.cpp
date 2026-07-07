#include "LocoUI.h"
#include "Theme.h"
#include <math.h>
#include <FileSystems.h>
#include <SD.h>
#include <Settings.h>
#include <StreamUtils.h>

LV_FONT_DECLARE(fa_icons_18);


LocoUI::LocoUI(DCCEXProtocol& dccex, Locos& locos, lv_obj_t* parent)
    : _dccex(dccex), _locos(locos), _fnPage(0) {

    _loco.address = (uint16_t)locos;

    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(_container, 0, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_container, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

    _keyboard = nullptr;
    _textarea = nullptr;
    _nameMenu = nullptr;
    _lockOverlay = nullptr;
    _speedArc = nullptr;
    _speedScale = nullptr;
    _speedLabel = nullptr;

    if (_loco.address == 0) {
        buildControlScreen();
        buildSelectionMenu();
    } else {
        buildControlScreen();
        buildSelectionMenu();
        _activeLoco = new Loco(_loco.address, LocoSourceEntry);
        _dccex.setThrottle(_activeLoco, 0, Direction::Forward);
        Serial.printf("[DCC] %s (%d) acquired\n", _locoName.c_str(), _loco.address);
    }
}

LocoUI::~LocoUI() {
    if (_activeLoco) {
        delete _activeLoco;
        _activeLoco = nullptr;
    }
    if (_keyboard) lv_obj_del(_keyboard);
    if (_textarea) lv_obj_del(_textarea);
    if (_container) lv_obj_del(_container);
}

void LocoUI::onLocoUpdate(Loco* loco) {
    if (!loco || loco->getAddress() != _loco.address) return;

    int newSpeed = loco->getSpeed();
    bool newDir = (loco->getDirection() == Direction::Forward);

    // Ignore stale CS echoes while the user is actively changing speed
    bool localHold = (millis() - _lastLocalSpeedMs) < SPEED_LOCAL_HOLD_MS;
    if (!localHold && _loco.speed != newSpeed) {
        lv_arc_set_value(_speedArc, newSpeed);
        lv_label_set_text_fmt(_speedLabel, "%d", newSpeed);
        if (_speedScale) lv_obj_invalidate(_speedScale);
        if (_dirBtn) lv_obj_set_style_opa(lv_obj_get_parent(_dirBtn), newSpeed == 0 ? LV_OPA_COVER : LV_OPA_40, 0);
        _loco.speed = newSpeed;
    }

    if (_loco.direction != newDir) {
        if (_dirBtn) {
            if (newDir) lv_obj_add_state(_dirBtn, LV_STATE_CHECKED);
            else        lv_obj_remove_state(_dirBtn, LV_STATE_CHECKED);
        }
        if (_dirFwdLabel) lv_obj_set_style_text_color(_dirFwdLabel, newDir ? tc(TC_TEXT_PRIMARY) : tc(TC_TEXT_MUTED), 0);
        if (_dirRevLabel) lv_obj_set_style_text_color(_dirRevLabel, newDir ? tc(TC_TEXT_MUTED) : tc(TC_TEXT_PRIMARY), 0);
        _loco.direction = newDir;
    }

    // Function state updates from CS broadcast
    for (int fn = 0; fn < 28; fn++) {
        bool csState = loco->isFunctionOn(fn);
        if (_loco.functions.test(fn) != csState) {
            _loco.functions.set(fn, csState);
            std::bitset<32> toggle;
            toggle.set(fn);
            toggleFunctionButtons(toggle);
        }
    }
}

void LocoUI::buildControlScreen() {
    // Holds the loco name + full functions array (nested btn icon/colour data).
    // Sized to fit a full DCC function set — too small and deserializeJson()
    // silently truncates the functions array, dropping buttons.
    DynamicJsonDocument locoDoc(8192);

    char path[32];
    sprintf(path, "/locos/%d.json", _loco.address);

    fs::FS& fs = Settings.getFS();

    if (_loco.address != 0 && fs.exists(path)) {
        File locoFile = fs.open(path);
        StaticJsonDocument<64> filterDoc;
        filterDoc["name"] = true;
        filterDoc["functions"] = true;
        ReadBufferingStream buffered(locoFile, 64);
        deserializeJson(locoDoc, buffered, DeserializationOption::Filter(filterDoc));
        locoFile.close();
    }

    if (!_activeConsist) {
        _locoName = "Unknown Loco";
        if (locoDoc.containsKey("name")) _locoName = locoDoc["name"].as<const char*>();
    }

    _nameLabel = lv_label_create(_container);
    lv_label_set_text(_nameLabel, _locoName.c_str());
    lv_obj_set_style_text_color(_nameLabel, tc(TC_TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(_nameLabel, &lv_font_montserrat_12, 0);
    lv_obj_align(_nameLabel, LV_ALIGN_TOP_MID, 0, us(4));

    _prevBtn = lv_btn_create(_container);
    lv_obj_t* prev_btn = _prevBtn;
    lv_obj_set_size(prev_btn, us(36), us(36));
    lv_obj_align(prev_btn, LV_ALIGN_TOP_LEFT, us(2), us(11));
    lv_obj_set_style_bg_color(prev_btn, tc(TC_SURFACE_RAISED), 0);
    lv_obj_set_style_border_color(prev_btn, tc(TC_BORDER_STRONG), 0);
    lv_obj_set_style_border_width(prev_btn, 1, 0);
    lv_obj_set_style_shadow_width(prev_btn, 0, 0);
    lv_obj_set_style_shadow_opa(prev_btn, LV_OPA_40, 0);
    lv_obj_set_style_radius(prev_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(prev_btn, 0, 0);
    lv_obj_t* pl = lv_label_create(prev_btn);
    lv_label_set_text(pl, "\xEF\x81\xA0");  // FA arrow-left U+F060
    lv_obj_set_style_text_font(pl, &fa_icons_18, 0);
    lv_obj_set_style_text_color(pl, tc(TC_TEXT_SECONDARY), 0);
    lv_obj_center(pl);
    lv_obj_add_event_cb(prev_btn, nav_btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(prev_btn, (void*)0);

    lv_obj_t* addr_btn = lv_btn_create(_container);
    lv_obj_set_style_bg_opa(addr_btn, 0, 0);
    lv_obj_set_style_shadow_width(addr_btn, 0, 0);
    lv_obj_add_event_cb(addr_btn, open_selection_event_cb, LV_EVENT_CLICKED, this);

    _addressLabel = lv_label_create(addr_btn);

    if (_activeConsist) {
        // Build "#6464FF 3# • #AAAAAA 45# • #AAAAAA 678#" with lead in blue, rest in grey
        char addrStr[256] = {0};
        int addrOff = 0;
        bool first = true;
        for (CSConsistMember* m = _activeConsist->getFirstMember(); m && addrOff < 250; m = m->next) {
            if (!first) addrOff += snprintf(addrStr + addrOff, sizeof(addrStr) - addrOff, " \xE2\x80\xA2 ");
            addrOff += snprintf(addrStr + addrOff, sizeof(addrStr) - addrOff,
                                first ? "#6464FF %d#" : "#AAAAAA %d#", m->address);
            first = false;
        }
        lv_obj_set_size(addr_btn, us(150), us(40));
        lv_obj_set_style_pad_all(addr_btn, 0, 0);
        lv_obj_set_style_pad_hor(addr_btn, us(4), 0);
        lv_obj_set_size(_addressLabel, us(142), LV_SIZE_CONTENT);
        lv_label_set_long_mode(_addressLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_duration(_addressLabel, 16000, 0);
        lv_label_set_recolor(_addressLabel, true);
        lv_label_set_text(_addressLabel, addrStr);
        lv_obj_set_style_text_font(_addressLabel, &lv_font_montserrat_16, 0);
    } else if (_loco.address > 0) {
        lv_label_set_text_fmt(_addressLabel, "%d", _loco.address);
        lv_obj_set_style_text_font(_addressLabel, &lv_font_montserrat_28, 0);
        lv_obj_set_size(addr_btn, us(100), us(40));
    } else {
        lv_label_set_text(_addressLabel, "None");
        lv_obj_set_style_text_font(_addressLabel, &lv_font_montserrat_28, 0);
        lv_obj_set_size(addr_btn, us(100), us(40));
    }

    lv_obj_align(addr_btn, LV_ALIGN_TOP_MID, 0, us(14));
    if (!_activeConsist)
        lv_obj_set_style_text_color(_addressLabel, tc(TC_TEXT_PRIMARY), 0);
    lv_obj_center(_addressLabel);

    _nextBtn = lv_btn_create(_container);
    lv_obj_t* next_btn = _nextBtn;
    lv_obj_set_size(next_btn, us(36), us(36));
    lv_obj_align(next_btn, LV_ALIGN_TOP_RIGHT, us(-2), us(11));
    lv_obj_set_style_bg_color(next_btn, tc(TC_SURFACE_RAISED), 0);
    lv_obj_set_style_border_color(next_btn, tc(TC_BORDER_STRONG), 0);
    lv_obj_set_style_border_width(next_btn, 1, 0);
    lv_obj_set_style_shadow_width(next_btn, 0, 0);
    lv_obj_set_style_shadow_opa(next_btn, LV_OPA_40, 0);
    lv_obj_set_style_radius(next_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(next_btn, 0, 0);
    lv_obj_t* nl = lv_label_create(next_btn);
    lv_label_set_text(nl, "\xEF\x81\xA1");  // FA arrow-right U+F061
    lv_obj_set_style_text_font(nl, &fa_icons_18, 0);
    lv_obj_set_style_text_color(nl, tc(TC_TEXT_SECONDARY), 0);
    lv_obj_center(nl);
    lv_obj_add_event_cb(next_btn, nav_btn_event_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(next_btn, (void*)1);

    if (_locos.count() <= 1) {
        lv_obj_add_flag(_prevBtn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_nextBtn, LV_OBJ_FLAG_HIDDEN);
    }

    buildGauge();

    // Large circular stop button — bottom right
    lv_obj_t* estop_btn = lv_btn_create(_container);
    lv_obj_set_size(estop_btn, usw(52), usw(52));
    lv_obj_align(estop_btn, LV_ALIGN_BOTTOM_RIGHT, usw(-6), usw(-6));
    lv_obj_set_style_radius(estop_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(estop_btn, tc(TC_DANGER), 0);
    lv_obj_set_style_bg_color(estop_btn, tc(TC_DANGER), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(estop_btn, 0, 0);
    lv_obj_set_style_shadow_opa(estop_btn, LV_OPA_40, 0);
    lv_obj_t* el = lv_label_create(estop_btn);
    lv_label_set_text(el, LV_SYMBOL_STOP);
    lv_obj_set_style_text_color(el, lv_color_hex(0xffffff), 0);
    lv_obj_center(el);
    lv_obj_add_event_cb(estop_btn, estop_btn_event_cb, LV_EVENT_CLICKED, this);

    // Direction toggle — FWD label | switch | REV label
    lv_obj_t* dir_row = lv_obj_create(_container);
    lv_obj_set_size(dir_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(dir_row, LV_ALIGN_BOTTOM_MID, 0, usw(-19));
    lv_obj_set_style_bg_opa(dir_row, 0, 0);
    lv_obj_set_style_border_width(dir_row, 0, 0);
    lv_obj_set_style_pad_all(dir_row, 0, 0);
    lv_obj_set_style_pad_column(dir_row, usw(6), 0);
    lv_obj_set_flex_flow(dir_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dir_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(dir_row, LV_OBJ_FLAG_SCROLLABLE);

    _dirRevLabel = lv_label_create(dir_row);
    lv_label_set_text(_dirRevLabel, "REV");
    lv_obj_set_style_text_font(_dirRevLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_dirRevLabel, _loco.direction ? tc(TC_TEXT_MUTED) : tc(TC_TEXT_PRIMARY), 0);

    _dirBtn = lv_switch_create(dir_row);
    lv_obj_set_size(_dirBtn, usw(52), usw(26));
    // unchecked = REV (yellow track), checked = FWD (green indicator)
    lv_obj_set_style_bg_color(_dirBtn, lv_color_make(180, 150, 30), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_dirBtn, lv_color_make(40, 180, 40),  (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (_loco.direction) lv_obj_add_state(_dirBtn, LV_STATE_CHECKED);
    lv_obj_add_event_cb(_dirBtn, dir_btn_event_cb, LV_EVENT_VALUE_CHANGED, this);

    _dirFwdLabel = lv_label_create(dir_row);
    lv_label_set_text(_dirFwdLabel, "FWD");
    lv_obj_set_style_text_font(_dirFwdLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_dirFwdLabel, _loco.direction ? tc(TC_TEXT_PRIMARY) : tc(TC_TEXT_MUTED), 0);

    // Fn page button — circle on the left, mirroring the stop button
    _pageBtn = lv_btn_create(_container);
    lv_obj_t* page_btn = _pageBtn;
    lv_obj_set_size(page_btn, usw(52), usw(52));
    lv_obj_align(page_btn, LV_ALIGN_BOTTOM_LEFT, usw(6), usw(-6));
    lv_obj_set_style_radius(page_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(page_btn, tc(TC_SURFACE_RAISED), 0);
    lv_obj_set_style_bg_color(page_btn, tc(TC_BORDER_STRONG), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(page_btn, tc(TC_BORDER_STRONG), 0);
    lv_obj_set_style_border_width(page_btn, 1, 0);
    lv_obj_set_style_shadow_width(page_btn, 0, 0);
    lv_obj_set_style_shadow_opa(page_btn, LV_OPA_40, 0);
    _pageBtnLabel = lv_label_create(page_btn);
    lv_label_set_text(_pageBtnLabel, "Fn");
    lv_obj_center(_pageBtnLabel);
    lv_obj_add_event_cb(page_btn, page_btn_event_cb, LV_EVENT_CLICKED, this);

    buildFunctionButtons(locoDoc);
    renderFunctionPage();

    // Lock overlay — shown when no loco is selected
    _lockOverlay = lv_obj_create(_container);
    lv_obj_set_size(_lockOverlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_lockOverlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(_lockOverlay, LV_OPA_90, 0);
    lv_obj_set_style_bg_color(_lockOverlay, tc(TC_GAUGE_BG), 0);
    lv_obj_set_style_border_width(_lockOverlay, 0, 0);
    lv_obj_set_style_radius(_lockOverlay, 0, 0);
    lv_obj_set_flex_flow(_lockOverlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_lockOverlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_lockOverlay, us(8), 0);
    lv_obj_clear_flag(_lockOverlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(_lockOverlay, open_selection_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* lock_icon = lv_label_create(_lockOverlay);
    lv_label_set_text(lock_icon, "\xEF\x80\xA3");  // FA lock U+F023
    lv_obj_set_style_text_font(lock_icon, &fa_icons_18, 0);
    lv_obj_set_style_text_color(lock_icon, tc(TC_TEXT_HINT), 0);

    lv_obj_t* lock_lbl = lv_label_create(_lockOverlay);
    lv_label_set_text(lock_lbl, "Select Locomotive");
    lv_obj_set_style_text_font(lock_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lock_lbl, tc(TC_TEXT_SECONDARY), 0);

    if (_loco.address != 0) lv_obj_add_flag(_lockOverlay, LV_OBJ_FLAG_HIDDEN);
}

void LocoUI::refresh() {
    lv_obj_clean(_container);
    _fnButtons.clear();
    _pageBtn = nullptr;
    _pageBtnLabel = nullptr;
    _prevBtn = nullptr;
    _nextBtn = nullptr;
    _lockOverlay = nullptr;

    if (_activeConsist) {
        uint16_t newAddr = (uint16_t)_locos;
        if (newAddr == _loco.address) {
            // Still on the consist's slot — keep driving
            buildControlScreen();
            buildSelectionMenu();
            return;
        }
        // Navigated away — release the consist
        _dccex.setThrottle(_activeConsist, 0, Direction::Forward);
        _dccex.deleteCSConsist(_activeConsist);
        _activeConsist = nullptr;
    }

    uint16_t newAddr = (uint16_t)_locos;
    if (_loco.address != newAddr) {
        _loco.address = newAddr;
        _loco.speed = 0;
        _loco.direction = true;
        _loco.functions.reset();
    }

    if (_activeLoco) {
        delete _activeLoco;
        _activeLoco = nullptr;
    }

    if (_loco.address == 0) {
        buildControlScreen();
        buildSelectionMenu();
    } else {
        buildControlScreen();
        buildSelectionMenu();
        _activeLoco = new Loco(_loco.address, LocoSourceEntry);
        _dccex.setThrottle(_activeLoco, 0, Direction::Forward);
        Serial.printf("[DCC] %s (%d) acquired\n", _locoName.c_str(), _loco.address);
    }
}

void LocoUI::onConsistUpdate(int leadLoco, CSConsist* consist) {
    if (!_activeConsist || _activeConsist != consist) return;
    bool localHold = (millis() - _lastLocalSpeedMs) < SPEED_LOCAL_HOLD_MS;
    if (!localHold) {
        int s = consist->getSpeed();
        if (_loco.speed != s) {
            _loco.speed = s;
            if (_speedArc) lv_arc_set_value(_speedArc, s);
            if (_speedLabel) lv_label_set_text_fmt(_speedLabel, "%d", s);
        }
    }
}

void LocoUI::nav_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    intptr_t action = (intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
    if (action == 0) ui->_locos.prev();
    else             ui->_locos.next();
    ui->refresh();
}

void LocoUI::dir_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_loco.speed != 0) {
        // Revert the switch back — don't allow direction change at speed
        if (ui->_loco.direction)
            lv_obj_add_state(ui->_dirBtn, LV_STATE_CHECKED);
        else
            lv_obj_remove_state(ui->_dirBtn, LV_STATE_CHECKED);
        return;
    }
    bool fwd = lv_obj_has_state(ui->_dirBtn, LV_STATE_CHECKED);
    ui->_loco.direction = fwd;
    if (ui->_dirFwdLabel) lv_obj_set_style_text_color(ui->_dirFwdLabel, fwd ? tc(TC_TEXT_PRIMARY) : tc(TC_TEXT_MUTED), 0);
    if (ui->_dirRevLabel) lv_obj_set_style_text_color(ui->_dirRevLabel, fwd ? tc(TC_TEXT_MUTED) : tc(TC_TEXT_PRIMARY), 0);
    if (ui->_activeConsist) {
        ui->_dccex.setThrottle(ui->_activeConsist, 0, fwd ? Direction::Forward : Direction::Reverse);
    } else if (ui->_activeLoco) {
        ui->_dccex.setThrottle(ui->_activeLoco, 0, fwd ? Direction::Forward : Direction::Reverse);
    }
}

void LocoUI::estop_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    ui->_loco.speed = 0;
    if (ui->_activeConsist) {
        ui->_dccex.setThrottle(ui->_activeConsist, 0,
            ui->_loco.direction ? Direction::Forward : Direction::Reverse);
    } else if (ui->_activeLoco) {
        ui->_dccex.setThrottle(ui->_activeLoco, 0,
            ui->_loco.direction ? Direction::Forward : Direction::Reverse);
    }
    if (ui->_speedArc)   lv_arc_set_value(ui->_speedArc, 0);
    if (ui->_speedLabel) lv_label_set_text(ui->_speedLabel, "0");
    if (ui->_speedScale) lv_obj_invalidate(ui->_speedScale);
    if (ui->_dirBtn)     lv_obj_set_style_opa(lv_obj_get_parent(ui->_dirBtn), LV_OPA_COVER, 0);
}

// void LocoUI::demoStep(int step) {
//     if (!_speedArc || !_dirBtn) return;
//     auto setSpeed = [this](int speed) {
//         lv_arc_set_value(_speedArc, speed);
//         lv_label_set_text_fmt(_speedLabel, "%d", speed);
//         lv_color_t color;
//         if (speed < 42)      color = lv_color_make(50, 255, 50);
//         else if (speed < 84) color = lv_color_make(255, 255, 50);
//         else                 color = lv_color_make(255, 50, 50);
//         lv_obj_set_style_arc_color(_speedArc, color, LV_PART_INDICATOR);
//         lv_obj_set_style_opa(lv_obj_get_parent(_dirBtn), speed == 0 ? LV_OPA_COVER : LV_OPA_40, 0);
//     };
//     auto setDir = [this](bool fwd) {
//         if (fwd) lv_obj_add_state(_dirBtn, LV_STATE_CHECKED);
//         else     lv_obj_remove_state(_dirBtn, LV_STATE_CHECKED);
//         lv_obj_set_style_text_color(_dirFwdLabel, fwd ? tc(TC_TEXT_PRIMARY) : tc(TC_TEXT_MUTED), 0);
//         lv_obj_set_style_text_color(_dirRevLabel, fwd ? tc(TC_TEXT_MUTED) : tc(TC_TEXT_PRIMARY), 0);
//     };
//     switch (step) {
//         case 0: if (_lockOverlay) lv_obj_clear_flag(_lockOverlay, LV_OBJ_FLAG_HIDDEN); setSpeed(0); setDir(true); break;
//         case 1: if (_lockOverlay) lv_obj_add_flag(_lockOverlay, LV_OBJ_FLAG_HIDDEN); lv_label_set_text(_addressLabel, "3"); lv_label_set_text(_nameLabel, "Steam Loco"); setSpeed(0); setDir(true); break;
//         case 2: setSpeed(42);   break;
//         case 3: setSpeed(126);  break;
//         case 4: setSpeed(0);    break;
//         case 5: setDir(false);  break;
//     }
// }

void LocoUI::nudgeSpeed(int delta) {
    if (!_speedArc) return;
    int speed = constrain((int)lv_arc_get_value(_speedArc) + delta, 0, 126);
    lv_arc_set_value(_speedArc, speed);
    if (_speedLabel) lv_label_set_text_fmt(_speedLabel, "%d", speed);
    if (_speedScale) lv_obj_invalidate(_speedScale);
    if (_dirBtn) lv_obj_set_style_opa(lv_obj_get_parent(_dirBtn), speed == 0 ? LV_OPA_COVER : LV_OPA_40, 0);
    _loco.speed = speed;
    _lastLocalSpeedMs = millis();
    if (_activeConsist) {
        _dccex.setThrottle(_activeConsist, speed,
            _loco.direction ? Direction::Forward : Direction::Reverse);
    } else if (_activeLoco) {
        _dccex.setThrottle(_activeLoco, speed,
            _loco.direction ? Direction::Forward : Direction::Reverse);
    }
}
