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

void LocoUI::buildSelectionMenu() {
    _selectionMenu = lv_obj_create(_container);
    lv_obj_set_width(_selectionMenu, LV_PCT(90));
#if defined(TFT_HEIGHT) && TFT_HEIGHT >= 480
    // 3.5": the taller panel fits everything — size the menu snugly to its
    // content (capped) so it never scrolls or bounces.
    lv_obj_set_height(_selectionMenu, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(_selectionMenu, LV_PCT(95), 0);
#else
    // CYD: content can exceed the small screen; keep a capped height and scroll.
    lv_obj_set_height(_selectionMenu, LV_PCT(95));
#endif
    lv_obj_align(_selectionMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_selectionMenu, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_color(_selectionMenu, tc(TC_OVERLAY_BORDER), 0);
    lv_obj_set_style_border_width(_selectionMenu, 1, 0);
    lv_obj_set_style_radius(_selectionMenu, us(8), 0);
    lv_obj_set_style_pad_all(_selectionMenu, us(8), 0);
    lv_obj_set_style_pad_row(_selectionMenu, us(5), 0);
    lv_obj_set_flex_flow(_selectionMenu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_selectionMenu, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
#if defined(TFT_HEIGHT) && TFT_HEIGHT >= 480
    // 3.5": no scrolling — content is sized to fit.
    lv_obj_clear_flag(_selectionMenu, LV_OBJ_FLAG_SCROLLABLE);
#else
    // CYD: allow vertical scrolling so the Recent chips can't clip Release.
    lv_obj_set_scroll_dir(_selectionMenu, LV_DIR_VER);
#endif
    lv_obj_add_flag(_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title_row = lv_obj_create(_selectionMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, us(30));
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select Locomotive");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, us(28));
    lv_obj_set_style_pad_hor(close_btn, us(10), 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, close_selection_event_cb, LV_EVENT_CLICKED, this);

    auto make_menu_btn = [&](const char* label, lv_event_cb_t cb, lv_color_t bg) {
        lv_obj_t* btn = lv_btn_create(_selectionMenu);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_height(btn, us(30));
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

    // Recent locos — quick-recall chips (persisted across sessions)
    if (Settings.recentLocoCount > 0) {
        lv_obj_t* rec_lbl = lv_label_create(_selectionMenu);
        lv_label_set_text(rec_lbl, "Recent");
        lv_obj_set_style_text_color(rec_lbl, tc(TC_TEXT_HINT), 0);
        lv_obj_set_style_text_font(rec_lbl, &lv_font_montserrat_10, 0);

        lv_obj_t* rec_row = lv_obj_create(_selectionMenu);
        lv_obj_set_width(rec_row, LV_PCT(100));
        lv_obj_set_height(rec_row, LV_SIZE_CONTENT);
        lv_obj_set_style_max_height(rec_row, us(72), 0);
        lv_obj_set_style_pad_all(rec_row, 0, 0);
        lv_obj_set_style_pad_row(rec_row, us(4), 0);
        lv_obj_set_style_pad_column(rec_row, us(4), 0);
        lv_obj_set_style_border_width(rec_row, 0, 0);
        lv_obj_set_style_bg_opa(rec_row, 0, 0);
        lv_obj_set_flex_flow(rec_row, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(rec_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

        for (uint8_t i = 0; i < Settings.recentLocoCount; i++) {
            uint16_t addr = Settings.recentLocos[i];
            lv_obj_t* chip = lv_btn_create(rec_row);
            lv_obj_set_height(chip, us(26));
            lv_obj_set_style_pad_hor(chip, us(8), 0);
            lv_obj_set_style_pad_ver(chip, 0, 0);
            lv_obj_set_style_radius(chip, us(13), 0);
            lv_obj_set_style_bg_color(chip, tc(TC_SURFACE_RAISED), 0);
            lv_obj_set_style_shadow_width(chip, 0, 0);
            lv_obj_set_user_data(chip, (void*)(uintptr_t)addr);
            lv_obj_add_event_cb(chip, recent_loco_event_cb, LV_EVENT_CLICKED, this);
            lv_obj_t* cl = lv_label_create(chip);
            lv_label_set_text_fmt(cl, "%d", addr);
            lv_obj_set_style_text_font(cl, &lv_font_montserrat_12, 0);
            lv_obj_center(cl);
        }
    }

    lv_obj_t* rel_btn = make_danger_btn(_selectionMenu, "Release");
    lv_obj_set_width(rel_btn, LV_PCT(100));
    lv_obj_set_height(rel_btn, us(30));
    lv_obj_add_event_cb(rel_btn, release_btn_event_cb, LV_EVENT_CLICKED, this);
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

    {
        lv_coord_t faceSize = (lv_coord_t)(lv_display_get_horizontal_resolution(lv_display_get_default()) * 45 / 100);
        _gaugeFaceRadius = faceSize / 2;

        // Vertical offset of the throttle from the container centre.
#if defined(TFT_HEIGHT) && TFT_HEIGHT >= 480
        // 3.5": centre the throttle on the mid-line of the 4 function buttons.
        lv_coord_t contentH = (lv_coord_t)(lv_display_get_vertical_resolution(lv_display_get_default()) - 70);
        int btnH = us(36);
        int fnCentre = (int)(contentH * fnTopOffset() / 250)
                     + (FN_ROWS - 1) * (int)(contentH * fnSpacing() / 250) / 2
                     + btnH / 2;
        int gaugeY = fnCentre - contentH / 2;
#else
        int gaugeY = us(13);
#endif

        // Dark gauge face — plain circle drawn as a sibling BEFORE the scale so
        // it sits behind the transparent scale widget. Ticks and labels on the
        // scale overflow outside this face boundary.
        lv_obj_t* gaugeFace = lv_obj_create(_container);
        lv_obj_set_size(gaugeFace, faceSize, faceSize);
        lv_obj_align(gaugeFace, LV_ALIGN_CENTER, 0, gaugeY);
        lv_obj_set_style_radius(gaugeFace, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(gaugeFace, tc(TC_GAUGE_BG), 0);
        lv_obj_set_style_bg_opa(gaugeFace, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(gaugeFace, 0, 0);
        lv_obj_set_style_shadow_width(gaugeFace, 0, 0);
        lv_obj_clear_flag(gaugeFace, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(gaugeFace, LV_OBJ_FLAG_CLICKABLE);

        // Scale widget — transparent, same size as face. ROUND_OUTER mode puts
        // labels outside the arc ring, which overflows the widget boundary.
        _speedScale = lv_scale_create(_container);
        lv_obj_set_size(_speedScale, faceSize, faceSize);
        lv_scale_set_mode(_speedScale, LV_SCALE_MODE_ROUND_OUTER);
        lv_scale_set_range(_speedScale, 0, 120);   // 0-120 display range; needle maps 0-126 → 0-120
        lv_scale_set_total_tick_count(_speedScale, 25);   // minor every 5 steps, 7 major ticks
        lv_scale_set_major_tick_every(_speedScale, 4);    // majors at 0,20,40,60,80,100,120
        lv_scale_set_angle_range(_speedScale, 270);
        lv_scale_set_rotation(_speedScale, 135);
        lv_obj_align(_speedScale, LV_ALIGN_CENTER, 0, gaugeY);
        lv_obj_clear_flag(_speedScale, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_speedScale, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

        // Transparent — face bg comes from the sibling gaugeFace behind
        lv_obj_set_style_bg_opa(_speedScale, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(_speedScale, 0, 0);
        lv_obj_set_style_pad_all(_speedScale, 0, 0);

        // Arc ring at face boundary
        lv_obj_set_style_arc_color(_speedScale, tc(TC_GAUGE_RING), LV_PART_MAIN);
        lv_obj_set_style_arc_width(_speedScale, 5, LV_PART_MAIN);

        // Major ticks + labels
        lv_obj_set_style_length(_speedScale, 12, LV_PART_INDICATOR);
        lv_obj_set_style_line_color(_speedScale, tc(TC_GAUGE_TICK_MAJOR), LV_PART_INDICATOR);
        lv_obj_set_style_line_width(_speedScale, 2, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(_speedScale, tc(TC_GAUGE_LABEL), LV_PART_INDICATOR);
        lv_obj_set_style_text_font(_speedScale, &lv_font_montserrat_12, LV_PART_INDICATOR);
        lv_obj_set_style_pad_radial(_speedScale, -3, LV_PART_INDICATOR);

        // Minor ticks
        lv_obj_set_style_length(_speedScale, 8, LV_PART_ITEMS);
        lv_obj_set_style_line_color(_speedScale, tc(TC_GAUGE_TICK_MINOR), LV_PART_ITEMS);
        lv_obj_set_style_line_width(_speedScale, 1, LV_PART_ITEMS);

        // Tapered needle — drawn as a filled triangle in DRAW_MAIN_END (before children)
        lv_obj_add_event_cb(_speedScale, gauge_needle_draw_cb, LV_EVENT_DRAW_MAIN_END, this);

        // Transparent arc on top — captures drag/touch for speed control
        _speedArc = lv_arc_create(_container);
        lv_obj_set_size(_speedArc, faceSize, faceSize);
        lv_arc_set_rotation(_speedArc, 135);
        lv_arc_set_bg_angles(_speedArc, 0, 270);
        lv_arc_set_range(_speedArc, 0, 126);
        lv_arc_set_value(_speedArc, _loco.speed);
        lv_obj_set_style_opa(_speedArc, LV_OPA_TRANSP, 0);
        lv_obj_align(_speedArc, LV_ALIGN_CENTER, 0, gaugeY);
        lv_obj_add_event_cb(_speedArc, speed_arc_event_cb, LV_EVENT_VALUE_CHANGED, this);

        // Masking circle — covers needle root so speed number reads cleanly
        lv_coord_t hubSize = faceSize * 42 / 100;
        lv_obj_t* hub = lv_obj_create(_speedScale);
        lv_obj_set_size(hub, hubSize, hubSize);
        lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(hub, tc(TC_GAUGE_BG), 0);
        lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(hub, 0, 0);
        lv_obj_set_style_shadow_color(hub, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_width(hub, 10, 0);
        lv_obj_set_style_shadow_spread(hub, 2, 0);
        lv_obj_set_style_shadow_opa(hub, LV_OPA_80, 0);
        lv_obj_set_style_shadow_offset_y(hub, 2, 0);
        lv_obj_clear_flag(hub, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);

        // Speed readout — parented to hub so it centres on the visible circle
        _speedLabel = lv_label_create(hub);
        lv_label_set_text_fmt(_speedLabel, "%d", _loco.speed);
        lv_obj_set_style_text_align(_speedLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(_speedLabel, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_speedLabel, tc(TC_GAUGE_NEEDLE), 0);
        lv_obj_center(_speedLabel);

    }

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

void LocoUI::showKeypad() {
  if (!_keyboard) {
    _keyboard = lv_obj_create(_container);
    lv_obj_set_size(_keyboard, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_keyboard, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_keyboard, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_width(_keyboard, 0, 0);
    lv_obj_set_style_pad_all(_keyboard, us(8), 0);
    lv_obj_set_style_pad_row(_keyboard, us(5), 0);
    lv_obj_set_flex_flow(_keyboard, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(_keyboard);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, us(36));
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Address");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, us(28));
    lv_obj_set_style_pad_hor(close_btn, us(10), 0);
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
    lv_obj_set_style_radius(ui->_nameMenu, us(8), 0);
    lv_obj_set_style_pad_all(ui->_nameMenu, us(8), 0);
    lv_obj_set_style_pad_row(ui->_nameMenu, us(5), 0);
    lv_obj_set_flex_flow(ui->_nameMenu, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, us(36));
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Name");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, us(28));
    lv_obj_set_style_pad_hor(close_btn, us(10), 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, close_name_menu_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t* list = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, us(4), 0);
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
                    lv_obj_set_height(btn, us(36));
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
        Settings.pushRecentLoco(address);
        ui->_nameMenu = nullptr;
        ui->_groupsDoc.reset();
        lv_async_call([](void* user_data) {
            ((LocoUI*)user_data)->refresh();
        }, ui);
    }
}

void LocoUI::recent_loco_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* chip = (lv_obj_t*)lv_event_get_target(e);
    uint16_t addr = (uintptr_t)lv_obj_get_user_data(chip);
    if (addr == 0) return;

    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
    ui->_locos.add(addr);
    Settings.pushRecentLoco(addr);
    lv_async_call([](void* user_data) {
        ((LocoUI*)user_data)->refresh();
    }, ui);
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
    lv_obj_set_style_radius(ui->_nameMenu, us(8), 0);
    lv_obj_set_style_pad_all(ui->_nameMenu, us(8), 0);
    lv_obj_set_style_pad_row(ui->_nameMenu, us(5), 0);
    lv_obj_set_flex_flow(ui->_nameMenu, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, us(36));
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Group");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, us(28));
    lv_obj_set_style_pad_hor(close_btn, us(10), 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, close_name_menu_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t* list = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, us(4), 0);
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
            lv_obj_set_height(btn, us(36));
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
            lv_obj_set_height(loco_btn, us(36));
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
                Settings.pushRecentLoco(addr);
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

void LocoUI::gauge_needle_draw_cb(lv_event_t* e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* scale = (lv_obj_t*)lv_event_get_target(e);
    lv_layer_t* layer = lv_event_get_layer(e);

    // Angle: rotation=135°, sweep=270°, display range 0-120 mapped from speed 0-126
    float displayVal = (float)ui->_loco.speed * 120.0f / 126.0f;
    float angleRad   = (135.0f + displayVal * 270.0f / 120.0f) * (float)M_PI / 180.0f;

    lv_area_t coords;
    lv_obj_get_coords(scale, &coords);
    float cx        = (coords.x1 + coords.x2) * 0.5f;
    float cy        = (coords.y1 + coords.y2) * 0.5f;
    float needleLen = (float)ui->_gaugeFaceRadius * 1.10f;
    float baseHalf  = 4.5f;

    float dx = cosf(angleRad), dy = sinf(angleRad); // along needle
    float px = -dy,            py =  dx;             // perpendicular

    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.p[0].x = cx + dx * needleLen;  dsc.p[0].y = cy + dy * needleLen; // tip
    dsc.p[1].x = cx + px * baseHalf;   dsc.p[1].y = cy + py * baseHalf;  // base +side
    dsc.p[2].x = cx - px * baseHalf;   dsc.p[2].y = cy - py * baseHalf;  // base −side
    dsc.color = tc(TC_GAUGE_NEEDLE);
    dsc.opa   = LV_OPA_COVER;
    lv_draw_triangle(layer, &dsc);
}

void LocoUI::speed_arc_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* arc = (lv_obj_t*)lv_event_get_target(e);
    int speed = lv_arc_get_value(arc);

    if (ui->_speedLabel) lv_label_set_text_fmt(ui->_speedLabel, "%d", speed);
    if (ui->_dirBtn) lv_obj_set_style_opa(lv_obj_get_parent(ui->_dirBtn), speed == 0 ? LV_OPA_COVER : LV_OPA_40, 0);
    if (ui->_speedScale) lv_obj_invalidate(ui->_speedScale);

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
