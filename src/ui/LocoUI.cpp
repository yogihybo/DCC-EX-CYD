#include "LocoUI.h"
#include "Theme.h"
#include <FileSystems.h>
#include <SD.h>
#include <Settings.h>
#include <StreamUtils.h>

LV_FONT_DECLARE(fa_icons_18);

// Encode a Unicode codepoint (U+0800..U+FFFF) as a null-terminated UTF-8 string
static void cp_to_utf8(uint32_t cp, char buf[5]) {
    if (cp < 0x800) {
        buf[0] = 0xC0 | (cp >> 6);
        buf[1] = 0x80 | (cp & 0x3F);
        buf[2] = '\0';
    } else {
        buf[0] = 0xE0 | (cp >> 12);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        buf[3] = '\0';
    }
}

static lv_color_t rgb565_to_lv(uint16_t c) {
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >>  5) & 0x3F;
    uint8_t b5 =  c        & 0x1F;
    return lv_color_make((r5 << 3) | (r5 >> 2),
                         (g6 << 2) | (g6 >> 4),
                         (b5 << 3) | (b5 >> 2));
}

LocoUI::LocoUI(DCCEXProtocol& dccex, Locos& locos, lv_obj_t* parent)
    : _dccex(dccex), _locos(locos), _fnPage(0) {

    _loco.address = (uint16_t)locos;

    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_pad_all(_container, 0, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);

    _keyboard = nullptr;
    _textarea = nullptr;
    _nameMenu = nullptr;
    _lockOverlay = nullptr;

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

void LocoUI::buildSelectionMenu() {
    _selectionMenu = lv_obj_create(_container);
    lv_obj_set_size(_selectionMenu, LV_PCT(90), LV_PCT(95));
    lv_obj_align(_selectionMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_selectionMenu, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_color(_selectionMenu, tc(TC_OVERLAY_BORDER), 0);
    lv_obj_set_style_border_width(_selectionMenu, 1, 0);
    lv_obj_set_style_radius(_selectionMenu, 8, 0);
    lv_obj_set_style_pad_all(_selectionMenu, 8, 0);
    lv_obj_set_style_pad_row(_selectionMenu, 5, 0);
    lv_obj_set_flex_flow(_selectionMenu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_selectionMenu, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(_selectionMenu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title_row = lv_obj_create(_selectionMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 30);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select Locomotive");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, 28);
    lv_obj_set_style_pad_hor(close_btn, 10, 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, close_selection_event_cb, LV_EVENT_CLICKED, this);

    auto make_menu_btn = [&](const char* label, lv_event_cb_t cb, lv_color_t bg) {
        lv_obj_t* btn = lv_btn_create(_selectionMenu);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_height(btn, 30);
        lv_obj_set_style_bg_color(btn, bg, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, this);
    };

    make_menu_btn("By Address", addr_btn_event_cb,    tc(TC_SURFACE_RAISED));
    make_menu_btn("By Name",    name_btn_event_cb,    tc(TC_SURFACE_RAISED));
    make_menu_btn("By Group",   group_btn_event_cb,   tc(TC_SURFACE_RAISED));
    make_menu_btn("By Consist", consist_btn_event_cb, tc(TC_SURFACE_RAISED));
    lv_obj_t* rel_btn = make_danger_btn(_selectionMenu, "Release");
    lv_obj_set_width(rel_btn, LV_PCT(100));
    lv_obj_set_height(rel_btn, 30);
    lv_obj_add_event_cb(rel_btn, release_btn_event_cb, LV_EVENT_CLICKED, this);
}

void LocoUI::buildControlScreen() {
    DynamicJsonDocument locoDoc(4096);

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
    lv_obj_align(_nameLabel, LV_ALIGN_TOP_MID, 0, 4);

    _prevBtn = lv_btn_create(_container);
    lv_obj_t* prev_btn = _prevBtn;
    lv_obj_set_size(prev_btn, 36, 36);
    lv_obj_align(prev_btn, LV_ALIGN_TOP_LEFT, 2, 11);
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
        lv_obj_set_size(addr_btn, 150, 40);
        lv_obj_set_style_pad_all(addr_btn, 0, 0);
        lv_obj_set_style_pad_hor(addr_btn, 4, 0);
        lv_obj_set_size(_addressLabel, 142, LV_SIZE_CONTENT);
        lv_label_set_long_mode(_addressLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_anim_duration(_addressLabel, 16000, 0);
        lv_label_set_recolor(_addressLabel, true);
        lv_label_set_text(_addressLabel, addrStr);
        lv_obj_set_style_text_font(_addressLabel, &lv_font_montserrat_16, 0);
    } else if (_loco.address > 0) {
        lv_label_set_text_fmt(_addressLabel, "%d", _loco.address);
        lv_obj_set_style_text_font(_addressLabel, &lv_font_montserrat_28, 0);
        lv_obj_set_size(addr_btn, 100, 40);
    } else {
        lv_label_set_text(_addressLabel, "None");
        lv_obj_set_style_text_font(_addressLabel, &lv_font_montserrat_28, 0);
        lv_obj_set_size(addr_btn, 100, 40);
    }

    lv_obj_align(addr_btn, LV_ALIGN_TOP_MID, 0, 14);
    if (!_activeConsist)
        lv_obj_set_style_text_color(_addressLabel, tc(TC_TEXT_PRIMARY), 0);
    lv_obj_center(_addressLabel);

    _nextBtn = lv_btn_create(_container);
    lv_obj_t* next_btn = _nextBtn;
    lv_obj_set_size(next_btn, 36, 36);
    lv_obj_align(next_btn, LV_ALIGN_TOP_RIGHT, -2, 11);
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

    _speedArc = lv_arc_create(_container);
    {
        lv_coord_t arcSize = (lv_coord_t)(lv_display_get_horizontal_resolution(lv_display_get_default()) * 55 / 100);
        lv_obj_set_size(_speedArc, arcSize, arcSize);
    }
    lv_arc_set_rotation(_speedArc, 135);
    lv_arc_set_bg_angles(_speedArc, 0, 270);
    lv_arc_set_range(_speedArc, 0, 126);
    lv_arc_set_value(_speedArc, _loco.speed);

    lv_obj_set_style_arc_color(_speedArc, tc(TC_SURFACE_RAISED), LV_PART_MAIN);
    lv_obj_set_style_arc_width(_speedArc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_speedArc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_speedArc, lv_color_hex(0x4488ff), LV_PART_KNOB);
    lv_obj_set_style_pad_all(_speedArc, 4, LV_PART_KNOB);
    lv_obj_set_style_arc_color(_speedArc, lv_color_hex(0x4488ff), LV_PART_INDICATOR);

    lv_obj_align(_speedArc, LV_ALIGN_CENTER, 0, 8);
    lv_obj_add_event_cb(_speedArc, speed_arc_event_cb, LV_EVENT_VALUE_CHANGED, this);

    _speedLabel = lv_label_create(_speedArc);
    lv_label_set_text_fmt(_speedLabel, "%d", _loco.speed);
    lv_obj_set_style_text_align(_speedLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(_speedLabel, &lv_font_montserrat_28, 0);
    lv_obj_align(_speedLabel, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t* unitLbl = lv_label_create(_speedArc);
    lv_label_set_text(unitLbl, "km/h");
    lv_obj_set_style_text_font(unitLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(unitLbl, tc(TC_TEXT_HINT), 0);
    lv_obj_align(unitLbl, LV_ALIGN_CENTER, 0, 14);

    // Large circular stop button — bottom right
    lv_obj_t* estop_btn = lv_btn_create(_container);
    lv_obj_set_size(estop_btn, 52, 52);
    lv_obj_align(estop_btn, LV_ALIGN_BOTTOM_RIGHT, -6, -6);
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
    lv_obj_align(dir_row, LV_ALIGN_BOTTOM_MID, 0, -19);
    lv_obj_set_style_bg_opa(dir_row, 0, 0);
    lv_obj_set_style_border_width(dir_row, 0, 0);
    lv_obj_set_style_pad_all(dir_row, 0, 0);
    lv_obj_set_style_pad_column(dir_row, 6, 0);
    lv_obj_set_flex_flow(dir_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dir_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(dir_row, LV_OBJ_FLAG_SCROLLABLE);

    _dirRevLabel = lv_label_create(dir_row);
    lv_label_set_text(_dirRevLabel, "REV");
    lv_obj_set_style_text_font(_dirRevLabel, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_dirRevLabel, _loco.direction ? tc(TC_TEXT_MUTED) : tc(TC_TEXT_PRIMARY), 0);

    _dirBtn = lv_switch_create(dir_row);
    lv_obj_set_size(_dirBtn, 52, 26);
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
    lv_obj_set_size(page_btn, 52, 52);
    lv_obj_align(page_btn, LV_ALIGN_BOTTOM_LEFT, 6, -6);
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
    lv_obj_set_style_bg_color(_lockOverlay, tc(TC_SURFACE_DEEP), 0);
    lv_obj_set_style_border_width(_lockOverlay, 0, 0);
    lv_obj_set_style_radius(_lockOverlay, 0, 0);
    lv_obj_set_flex_flow(_lockOverlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_lockOverlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_lockOverlay, 8, 0);
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

static const char* BUILTIN_PATHS[] = {
    "/www/fns/builtin-basic.json",
    "/www/fns/builtin-extended.json",
};

void LocoUI::buildFunctionButtons(JsonDocument& locoDoc) {
    JsonArrayConst locoFunctions;
    if (locoDoc["functions"].is<JsonArray>()) {
        locoFunctions = locoDoc["functions"].as<JsonArray>();
    } else {
        File functionFile;
        fs::FS& fs = Settings.getFS();
        bool builtinLoaded = false;

        if (locoDoc.containsKey("functions")) {
            const char* fnPath = locoDoc["functions"].as<const char*>();
            if (fnPath && strncmp(fnPath, "builtin:", 8) == 0) {
                int idx = atoi(fnPath + 8);
                if (idx >= 0 && idx < (int)(sizeof(BUILTIN_PATHS) / sizeof(BUILTIN_PATHS[0]))) {
                    File builtinFile = WebsiteFS.open(BUILTIN_PATHS[idx]);
                    if (builtinFile) {
                        StaticJsonDocument<32> filter;
                        filter["functions"] = true;
                        locoDoc.clear();
                        deserializeJson(locoDoc, builtinFile, DeserializationOption::Filter(filter));
                        builtinFile.close();
                        locoFunctions = locoDoc["functions"].as<JsonArray>();
                        builtinLoaded = true;
                    }
                }
            } else if (fnPath && fs.exists(fnPath)) {
                functionFile = fs.open(fnPath);
            }
        }
        if (!builtinLoaded && !functionFile) {
            if (WebsiteFS.exists("/default.json")) {
                functionFile = WebsiteFS.open("/default.json");
            } else if (fs.exists("/default.json")) {
                functionFile = fs.open("/default.json");
            }
        }

        if (functionFile) {
            StaticJsonDocument<32> filter;
            filter["functions"] = true;
            deserializeJson(locoDoc, functionFile, DeserializationOption::Filter(filter));
            locoFunctions = locoDoc["functions"].as<JsonArray>();
            functionFile.close();
        }
    }

    static lv_style_t fn_btn_base_style;
    lv_style_init(&fn_btn_base_style);
    lv_style_set_radius(&fn_btn_base_style, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&fn_btn_base_style, tc(TC_SURFACE_RAISED));
    lv_style_set_border_color(&fn_btn_base_style, tc(TC_BORDER_STRONG));
    lv_style_set_border_width(&fn_btn_base_style, 1);
    lv_style_set_shadow_width(&fn_btn_base_style, 0);
    lv_style_set_shadow_opa(&fn_btn_base_style, LV_OPA_40);

    for (JsonArrayConst const& row : locoFunctions) {
        for (JsonObjectConst const& fn : row) {
            uint8_t func = fn["fn"];
            bool latching = fn["latching"] | true;
            const char* idle_label   = fn["btn"]["idle"]["label"].as<const char*>();
            const char* pressed_label = fn["btn"]["pressed"]["label"].as<const char*>();
            uint32_t idle_cp    = fn["btn"]["idle"]["icon"]    | (uint32_t)0;
            uint32_t pressed_cp = fn["btn"]["pressed"]["icon"] | (uint32_t)0;

            lv_color_t idle_fill   = rgb565_to_lv(fn["btn"]["idle"]["fill"]     | (uint16_t)0x0000);
            lv_color_t idle_border = rgb565_to_lv(fn["btn"]["idle"]["border"]   | (uint16_t)0xFFFF);
            lv_color_t idle_color  = rgb565_to_lv(fn["btn"]["idle"]["color"]    | (uint16_t)0xFFFF);
            lv_color_t press_fill   = rgb565_to_lv(fn["btn"]["pressed"]["fill"]   | (uint16_t)0xFFFF);
            lv_color_t press_border = rgb565_to_lv(fn["btn"]["pressed"]["border"] | (uint16_t)0xFFFF);
            lv_color_t press_color  = rgb565_to_lv(fn["btn"]["pressed"]["color"]  | (uint16_t)0x0000);

            lv_obj_t* btn = lv_btn_create(_container);
            lv_obj_set_size(btn, 36, 36);
            lv_obj_add_style(btn, &fn_btn_base_style, 0);
            lv_obj_set_style_bg_color(btn, press_fill, LV_STATE_CHECKED);
            lv_obj_set_style_border_color(btn, press_color, LV_STATE_CHECKED);

            if (latching) {
                lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
            } else {
                lv_obj_set_style_bg_color(btn, press_fill, LV_STATE_PRESSED);
                lv_obj_set_style_border_color(btn, press_color, LV_STATE_PRESSED);
                auto swap_icons = [](lv_event_t* e) {
                    lv_obj_t* b = (lv_obj_t*)lv_event_get_target(e);
                    if (lv_obj_get_child_cnt(b) < 2) return;
                    bool pressing = (lv_event_get_code(e) == LV_EVENT_PRESSED);
                    lv_obj_t* show = lv_obj_get_child(b, pressing ? 1 : 0);
                    lv_obj_t* hide = lv_obj_get_child(b, pressing ? 0 : 1);
                    lv_obj_clear_flag(show, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(hide, LV_OBJ_FLAG_HIDDEN);
                };
                lv_obj_add_event_cb(btn, swap_icons, LV_EVENT_PRESSED,    nullptr);
                lv_obj_add_event_cb(btn, swap_icons, LV_EVENT_RELEASED,   nullptr);
                lv_obj_add_event_cb(btn, swap_icons, LV_EVENT_PRESS_LOST, nullptr);
            }

            auto create_visual = [btn, func](uint32_t cp, const char* label, lv_color_t col) -> lv_obj_t* {
                lv_obj_t* visual_obj = lv_label_create(btn);
                lv_obj_set_style_text_color(visual_obj, col, 0);
                if (cp > 0) {
                    char utf8[5] = {0};
                    cp_to_utf8(cp, utf8);
                    lv_obj_set_style_text_font(visual_obj, &fa_icons_18, 0);
                    lv_label_set_text(visual_obj, utf8);
                } else if (label && strlen(label) > 0) {
                    lv_obj_set_style_text_font(visual_obj, &lv_font_montserrat_10, 0);
                    lv_label_set_text(visual_obj, label);
                } else {
                    lv_obj_set_style_text_font(visual_obj, &lv_font_montserrat_10, 0);
                    lv_label_set_text_fmt(visual_obj, "F%d", func);
                }
                lv_obj_center(visual_obj);
                return visual_obj;
            };

            const uint32_t eff_press_cp    = pressed_cp ? pressed_cp : idle_cp;
            const char*    eff_press_label = (pressed_label && strlen(pressed_label) > 0) ? pressed_label : idle_label;

            lv_obj_t* idle_obj    = create_visual(idle_cp,      idle_label,       idle_color);
            lv_obj_t* pressed_obj = create_visual(eff_press_cp, eff_press_label,  press_color);

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

            lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
            _fnButtons.push_back(btn);
        }
    }
}

void LocoUI::renderFunctionPage() {
    for (auto btn : _fnButtons) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }

    int start_idx = _fnPage * 6;
    for (int i = 0; i < 6; i++) {
        int idx = start_idx + i;
        if (idx >= (int)_fnButtons.size()) break;

        lv_obj_t* btn = _fnButtons[idx];
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);

        lv_coord_t contentH = (lv_coord_t)(lv_display_get_vertical_resolution(lv_display_get_default()) - 70);
        int y_pos = (int)(contentH * 56 / 250) + (i % 3) * (int)(contentH * 44 / 250);
        if (i < 3) lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 2, y_pos);
        else        lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -2, y_pos);
    }

    if (_pageBtn && _pageBtnLabel) {
        int totalPages = ((int)_fnButtons.size() + 5) / 6;
        if (totalPages > 1) {
            lv_obj_clear_flag(_pageBtn, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(_pageBtnLabel, "Fn %d/%d", _fnPage + 1, totalPages);
        } else {
            lv_obj_add_flag(_pageBtn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void LocoUI::toggleFunctionButtons(std::bitset<32> toggle) {
    for (auto child : _fnButtons) {
        uint8_t func = (uintptr_t)lv_obj_get_user_data(child);
        if (toggle.test(func)) {
            bool is_checked = _loco.functions.test(func);
            if (is_checked) lv_obj_add_state(child, LV_STATE_CHECKED);
            else            lv_obj_clear_state(child, LV_STATE_CHECKED);
            if (lv_obj_get_child_cnt(child) >= 2) {
                lv_obj_t* idle_obj    = lv_obj_get_child(child, 0);
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
    lv_obj_set_style_bg_color(_keyboard, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_width(_keyboard, 0, 0);
    lv_obj_set_style_pad_all(_keyboard, 8, 0);
    lv_obj_set_style_pad_row(_keyboard, 5, 0);
    lv_obj_set_flex_flow(_keyboard, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(_keyboard);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 36);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Address");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, 28);
    lv_obj_set_style_pad_hor(close_btn, 10, 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);

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

void LocoUI::open_selection_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_move_foreground(ui->_selectionMenu);
    lv_obj_clear_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
}

void LocoUI::close_selection_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
}

void LocoUI::name_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    if (ui->_nameMenu) {
        lv_obj_delete_async(ui->_nameMenu);
        ui->_nameMenu = nullptr;
    }

    ui->_nameMenu = lv_obj_create(ui->_container);
    lv_obj_set_size(ui->_nameMenu, LV_PCT(90), LV_PCT(95));
    lv_obj_align(ui->_nameMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui->_nameMenu, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_color(ui->_nameMenu, tc(TC_OVERLAY_BORDER), 0);
    lv_obj_set_style_border_width(ui->_nameMenu, 1, 0);
    lv_obj_set_style_radius(ui->_nameMenu, 8, 0);
    lv_obj_set_style_pad_all(ui->_nameMenu, 8, 0);
    lv_obj_set_style_pad_row(ui->_nameMenu, 5, 0);
    lv_obj_set_flex_flow(ui->_nameMenu, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 36);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Name");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, 28);
    lv_obj_set_style_pad_hor(close_btn, 10, 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, close_name_menu_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t* list = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    auto addLocos = [ui, list](fs::FS& fs) {
        File dir = fs.open("/locos");
        if (dir) {
            StaticJsonDocument<16> filterDoc;
            filterDoc["name"] = true;
            StaticJsonDocument<64> locoDoc;
            while (File file = dir.openNextFile()) {
                if (!file.isDirectory()) {
                    uint16_t address = strtoul(file.name(), (char**)NULL, 10);

                    locoDoc.clear();
                    ReadBufferingStream buffered(file, 64);
                    deserializeJson(locoDoc, buffered, DeserializationOption::Filter(filterDoc));

                    char nameStr[56];
                    const char* locoName = locoDoc["name"] | "Unknown";
                    snprintf(nameStr, sizeof(nameStr), "#%d - %s", address, locoName);

                    lv_obj_t* btn = lv_btn_create(list);
                    lv_obj_set_width(btn, LV_PCT(100));
                    lv_obj_set_height(btn, 36);
                    lv_obj_set_style_bg_color(btn, tc(TC_SURFACE_RAISED), 0);
                    lv_obj_set_style_shadow_width(btn, 0, 0);
                    lv_obj_t* lbl = lv_label_create(btn);
                    lv_label_set_text(lbl, nameStr);
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
        ui->_groupsDoc.reset();
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
    ui->_groupsDoc.reset();
    if (ui->_selectionMenu) lv_obj_clear_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
}

void LocoUI::group_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    if (ui->_nameMenu) {
        lv_obj_delete_async(ui->_nameMenu);
        ui->_nameMenu = nullptr;
    }

    ui->_nameMenu = lv_obj_create(ui->_container);
    lv_obj_set_size(ui->_nameMenu, LV_PCT(90), LV_PCT(95));
    lv_obj_align(ui->_nameMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui->_nameMenu, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_color(ui->_nameMenu, tc(TC_OVERLAY_BORDER), 0);
    lv_obj_set_style_border_width(ui->_nameMenu, 1, 0);
    lv_obj_set_style_radius(ui->_nameMenu, 8, 0);
    lv_obj_set_style_pad_all(ui->_nameMenu, 8, 0);
    lv_obj_set_style_pad_row(ui->_nameMenu, 5, 0);
    lv_obj_set_flex_flow(ui->_nameMenu, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 36);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Group");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, 28);
    lv_obj_set_style_pad_hor(close_btn, 10, 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, close_name_menu_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t* list = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_style_border_width(list, 0, 0);

    fs::FS& fs = Settings.getFS();

    if (fs.exists("/groups.json")) {
        ui->_groupsDoc = std::make_unique<DynamicJsonDocument>(1024);
        File groupsFile = fs.open("/groups.json");
        ReadBufferingStream buffered(groupsFile, 64);
        deserializeJson(*ui->_groupsDoc, buffered);
        groupsFile.close();

        JsonArray groups = ui->_groupsDoc->as<JsonArray>();
        int index = 0;
        for (JsonObjectConst group : groups) {
            lv_obj_t* btn = lv_btn_create(list);
            lv_obj_set_width(btn, LV_PCT(100));
            lv_obj_set_height(btn, 36);
            lv_obj_set_style_bg_color(btn, tc(TC_SURFACE_RAISED), 0);
            lv_obj_set_style_shadow_width(btn, 0, 0);
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

    if (!ui->_groupsDoc) return;
    JsonArray groups = ui->_groupsDoc->as<JsonArray>();
    if (index >= 0 && index < groups.size()) {
        JsonObjectConst group = groups[index];
        JsonArrayConst locos = group["locos"];

        lv_obj_t* list = lv_obj_get_child(ui->_nameMenu, 1);
        lv_obj_clean(list);

        lv_obj_t* title_row = lv_obj_get_child(ui->_nameMenu, 0);
        lv_obj_t* title = lv_obj_get_child(title_row, 0);
        lv_label_set_text_fmt(title, "Group: %s", group["name"] | "Unknown");

        fs::FS& fs2 = Settings.getFS();
        StaticJsonDocument<16> filterDoc;
        filterDoc["name"] = true;
        StaticJsonDocument<64> locoDoc;
        for (uint16_t address : locos) {
            char path[32];
            sprintf(path, "/locos/%d.json", address);

            char nameStr[56];
            const char* locoName = "Missing";
            if (fs2.exists(path)) {
                locoDoc.clear();
                File locoFile = fs2.open(path);
                ReadBufferingStream buffered(locoFile, 64);
                deserializeJson(locoDoc, buffered, DeserializationOption::Filter(filterDoc));
                locoFile.close();
                locoName = locoDoc["name"] | "Unknown";
            }
            snprintf(nameStr, sizeof(nameStr), "#%d - %s", address, locoName);

            lv_obj_t* loco_btn = lv_btn_create(list);
            lv_obj_set_width(loco_btn, LV_PCT(100));
            lv_obj_set_height(loco_btn, 36);
            lv_obj_set_style_bg_color(loco_btn, tc(TC_SURFACE_RAISED), 0);
            lv_obj_set_style_shadow_width(loco_btn, 0, 0);
            lv_obj_t* lbl = lv_label_create(loco_btn);
            lv_label_set_text(lbl, nameStr);
            lv_obj_center(lbl);

            lv_obj_set_user_data(loco_btn, (void*)(uintptr_t)address);
            lv_obj_add_event_cb(loco_btn, loco_selected_event_cb, LV_EVENT_CLICKED, ui);
        }
    }
}

void LocoUI::release_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_activeConsist) {
        ui->_dccex.setThrottle(ui->_activeConsist, 0, Direction::Forward);
        ui->_dccex.deleteCSConsist(ui->_activeConsist);
        ui->_activeConsist = nullptr;
        Serial.printf("[DCC] Consist '%s' released\n", ui->_consistName.c_str());
        ui->_locos.remove();
        if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
        lv_async_call([](void* user_data) { ((LocoUI*)user_data)->refresh(); }, ui);
        return;
    }
    if (ui->_loco.address != 0) {
        if (ui->_activeLoco) {
            ui->_dccex.setThrottle(ui->_activeLoco, 0,
                ui->_loco.direction ? Direction::Forward : Direction::Reverse);
            char cmd[16];
            snprintf(cmd, sizeof(cmd), "- %d", ui->_loco.address);
            ui->_dccex.sendCommand(cmd);
            delete ui->_activeLoco;
            ui->_activeLoco = nullptr;
        }
        Serial.printf("[DCC] %s (%d) released\n", ui->_locoName.c_str(), ui->_loco.address);
        ui->_locos.remove();

        if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

        lv_async_call([](void* user_data) {
            ((LocoUI*)user_data)->refresh();
        }, ui);
    }
}

void LocoUI::consist_btn_event_cb(lv_event_t* e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    // ConsistUI is self-owned; it deletes itself on Close or Drive
    new ConsistUI(ui->_dccex, lv_layer_top(), [ui](CSConsist* consist, const String& name) {
        ui->_activeConsist = consist;
        ui->_activeLoco = nullptr;
        ui->_loco = {};
        ui->_consistName = name;
        ui->_locoName = name;
        CSConsistMember* lead = consist->getFirstMember();
        ui->_loco.address = lead ? lead->address : 0;
        ui->_locos.add(ui->_loco.address);
        Serial.printf("[DCC] Consist '%s' acquired (lead %d)\n", name.c_str(), ui->_loco.address);
        lv_async_call([](void* user_data) { ((LocoUI*)user_data)->refresh(); }, ui);
    });
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

void LocoUI::speed_arc_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* arc = (lv_obj_t*)lv_event_get_target(e);
    int speed = lv_arc_get_value(arc);

    if (ui->_speedLabel) lv_label_set_text_fmt(ui->_speedLabel, "%d", speed);
    if (ui->_dirBtn) lv_obj_set_style_opa(lv_obj_get_parent(ui->_dirBtn), speed == 0 ? LV_OPA_COVER : LV_OPA_40, 0);

    ui->_loco.speed = speed;
    ui->_lastLocalSpeedMs = millis();
    if (ui->_activeConsist) {
        ui->_dccex.setThrottle(ui->_activeConsist, speed,
            ui->_loco.direction ? Direction::Forward : Direction::Reverse);
    } else if (ui->_activeLoco) {
        ui->_dccex.setThrottle(ui->_activeLoco, speed,
            ui->_loco.direction ? Direction::Forward : Direction::Reverse);
    }
}

void LocoUI::fn_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    uint8_t func = (uintptr_t)lv_obj_get_user_data(btn);
    bool is_checked = lv_obj_has_state(btn, LV_STATE_CHECKED);

    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        if (lv_obj_get_child_cnt(btn) >= 2) {
            lv_obj_t* idle_obj    = lv_obj_get_child(btn, 0);
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

    if (ui->_activeConsist) {
        if (is_checked) ui->_dccex.functionOn(ui->_activeConsist, func);
        else            ui->_dccex.functionOff(ui->_activeConsist, func);
    } else if (ui->_activeLoco) {
        if (is_checked) ui->_dccex.functionOn(ui->_activeLoco, func);
        else            ui->_dccex.functionOff(ui->_activeLoco, func);
    }
}

void LocoUI::page_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    ui->_fnPage++;
    if (ui->_fnPage * 6 >= (int)ui->_fnButtons.size()) ui->_fnPage = 0;
    ui->renderFunctionPage();
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
    if (ui->_speedArc)   lv_obj_set_style_arc_color(ui->_speedArc, lv_color_make(50, 255, 50), LV_PART_INDICATOR);
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
    _loco.speed = speed;
    _lastLocalSpeedMs = millis();
    if (_speedLabel) lv_label_set_text_fmt(_speedLabel, "%d", speed);
    lv_color_t color;
    if (speed < 42)      color = lv_color_make(50, 255, 50);
    else if (speed < 84) color = lv_color_make(255, 255, 50);
    else                 color = lv_color_make(255, 50, 50);
    lv_obj_set_style_arc_color(_speedArc, color, LV_PART_INDICATOR);
    if (_activeConsist) {
        _dccex.setThrottle(_activeConsist, speed,
            _loco.direction ? Direction::Forward : Direction::Reverse);
    } else if (_activeLoco) {
        _dccex.setThrottle(_activeLoco, speed,
            _loco.direction ? Direction::Forward : Direction::Reverse);
    }
}
