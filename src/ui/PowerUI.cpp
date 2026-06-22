#include "PowerUI.h"
#include "Theme.h"

static lv_obj_t* make_card(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_set_style_pad_row(card, 5, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_bg_color(card, tc(TC_SURFACE), 0);
    lv_obj_set_style_radius(card, 6, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    return card;
}

static lv_obj_t* make_row(lv_obj_t* parent, int height) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, height);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 4, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

static lv_obj_t* make_dot(lv_obj_t* parent) {
    lv_obj_t* dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_bg_color(dot, tc(TC_ICON_INACTIVE), 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    return dot;
}

static lv_obj_t* make_action_btn(lv_obj_t* parent, const char* label, int action_id, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, 32);
    lv_obj_set_style_bg_color(btn, tc(TC_SURFACE_RAISED), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);
    lv_obj_set_user_data(btn, (void*)(uintptr_t)action_id);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}

PowerUI::PowerUI(DCCEXProtocol& dccex, lv_obj_t* parent) : _dccex(dccex) {
    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(_container, 5, 0);
    lv_obj_set_style_pad_row(_container, 5, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);

    // --- Main track card ---
    lv_obj_t* main_card = make_card(_container);

    lv_obj_t* main_hdr = make_row(main_card, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(main_hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_height(main_hdr, 18);

    lv_obj_t* main_name = lv_label_create(main_hdr);
    lv_label_set_text(main_name, "Main track");
    lv_obj_set_flex_grow(main_name, 1);

    _lbl_main_status = lv_label_create(main_hdr);
    _dot_main = make_dot(main_hdr);
    lv_obj_set_style_text_font(_lbl_main_status, &lv_font_montserrat_12, 0);

    lv_obj_t* main_btns = make_row(main_card, 32);
    _btn_main_on  = make_action_btn(main_btns, "ON",  1, btn_event_cb, this);
    _btn_main_off = make_action_btn(main_btns, "OFF", 2, btn_event_cb, this);

    // --- Prog track card ---
    lv_obj_t* prog_card = make_card(_container);

    lv_obj_t* prog_hdr = make_row(prog_card, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(prog_hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_height(prog_hdr, 18);

    lv_obj_t* prog_name = lv_label_create(prog_hdr);
    lv_label_set_text(prog_name, "Prog track");
    lv_obj_set_flex_grow(prog_name, 1);

    _lbl_prog_status = lv_label_create(prog_hdr);
    _dot_prog = make_dot(prog_hdr);
    lv_obj_set_style_text_font(_lbl_prog_status, &lv_font_montserrat_12, 0);

    lv_obj_t* prog_btns = make_row(prog_card, 32);
    _btn_prog_on  = make_action_btn(prog_btns, "ON",  3, btn_event_cb, this);
    _btn_prog_off = make_action_btn(prog_btns, "OFF", 4, btn_event_cb, this);

    // --- All tracks row ---
    lv_obj_t* all_row = make_row(_container, 32);
    lv_obj_set_style_pad_column(all_row, 5, 0);
    lv_obj_set_style_pad_hor(all_row, 6, 0);
    _btn_all_on  = make_action_btn(all_row, "All ON",  5, btn_event_cb, this);
    _btn_all_off = make_action_btn(all_row, "All OFF", 6, btn_event_cb, this);

    // --- Join tracks (de-emphasised) ---
    _btn_join = lv_btn_create(_container);
    lv_obj_set_width(_btn_join, LV_PCT(100));
    lv_obj_set_style_margin_hor(_btn_join, 6, 0);
    lv_obj_set_style_shadow_width(_btn_join, 0, 0);
    lv_obj_set_height(_btn_join, 32);
    lv_obj_set_style_bg_color(_btn_join, tc(TC_SURFACE_DEEP), 0);
    lv_obj_set_style_border_width(_btn_join, 1, 0);
    lv_obj_set_style_border_color(_btn_join, tc(TC_BORDER), 0);
    lv_obj_t* join_lbl = lv_label_create(_btn_join);
    lv_label_set_text(join_lbl, "Join tracks");
    lv_obj_set_style_text_color(join_lbl, tc(TC_TEXT_HINT), 0);
    lv_obj_center(join_lbl);
    lv_obj_set_user_data(_btn_join, (void*)7);
    lv_obj_add_event_cb(_btn_join, btn_event_cb, LV_EVENT_CLICKED, this);

    updateStyles();
}

PowerUI::~PowerUI() {
    if (_container) lv_obj_del(_container);
}

void PowerUI::updateStyles() {
    static const lv_color_t WHITE = lv_color_hex(0xffffff);
    static const lv_color_t GREEN_BG  = lv_color_make(40,  140, 40);
    static const lv_color_t RED_BG    = lv_color_make(140, 40,  40);
    static const lv_color_t GREEN_FG  = lv_color_make(76,  175, 80);
    static const lv_color_t RED_FG    = lv_color_make(180, 50,  50);

    auto set_btn = [&](lv_obj_t* btn, bool active, lv_color_t active_bg) {
        lv_obj_set_style_bg_color(btn, active ? active_bg : tc(TC_SURFACE_RAISED), 0);
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) lv_obj_set_style_text_color(lbl, active ? WHITE : tc(TC_TEXT_PRIMARY), 0);
    };

    set_btn(_btn_main_on,  _mainOn,  GREEN_BG);
    set_btn(_btn_main_off, !_mainOn, RED_BG);

    lv_obj_set_style_bg_color(_dot_main, _mainOn ? GREEN_FG : RED_FG, 0);
    lv_label_set_text(_lbl_main_status, _mainOn ? "ON" : "OFF");
    lv_obj_set_style_text_color(_lbl_main_status, _mainOn ? GREEN_FG : RED_FG, 0);

    set_btn(_btn_prog_on,  _progOn,  GREEN_BG);
    set_btn(_btn_prog_off, !_progOn, RED_BG);

    lv_obj_set_style_bg_color(_dot_prog, _progOn ? GREEN_FG : RED_FG, 0);
    lv_label_set_text(_lbl_prog_status, _progOn ? "ON" : "OFF");
    lv_obj_set_style_text_color(_lbl_prog_status, _progOn ? GREEN_FG : RED_FG, 0);

    set_btn(_btn_all_on,  (_mainOn && _progOn),   GREEN_BG);
    set_btn(_btn_all_off, (!_mainOn && !_progOn),  RED_BG);

    lv_obj_set_style_bg_color(_btn_join,
        _joinOn ? lv_color_make(180, 120, 30) : tc(TC_SURFACE_DEEP), 0);
    lv_obj_t* join_lbl = lv_obj_get_child(_btn_join, 0);
    if (join_lbl) {
        lv_label_set_text(join_lbl, _joinOn ? "Unjoin (cuts power)" : "Join tracks");
        lv_obj_set_style_text_color(join_lbl,
            _joinOn ? WHITE : tc(TC_TEXT_HINT), 0);
    }
}

void PowerUI::onPowerUpdate(bool main, bool prog, bool join) {
    _updatingFromBroadcast = true;
    _mainOn = main;
    _progOn = prog;
    _joinOn = join;

    Serial.printf("[DCC] Track power – Main: %s  Prog: %s  Join: %s\n",
        main ? "ON" : "OFF", prog ? "ON" : "OFF", join ? "ON" : "OFF");

    updateStyles();
    _updatingFromBroadcast = false;
}

void PowerUI::onIndividualPowerUpdate(TrackPower state, int track) {
    bool on = (state == TrackPower::PowerOn);
    _updatingFromBroadcast = true;

    if (track == 2698315) {
        _mainOn = on;
    } else if (track == 2788330) {
        _progOn = on;
    }

    updateStyles();
    _updatingFromBroadcast = false;
}

// void PowerUI::demoStep(int step) {
//     switch (step) {
//         case 0: onPowerUpdate(false, false, false); break;
//         case 1: onPowerUpdate(true,  false, false); break;
//         case 2: onPowerUpdate(true,  true,  false); break;
//     }
// }

void PowerUI::btn_event_cb(lv_event_t* e) {
    PowerUI* ui = (PowerUI*)lv_event_get_user_data(e);
    if (ui->_updatingFromBroadcast) return;

    int action = (int)(uintptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_current_target(e));

    switch (action) {
        case 1: ui->_dccex.powerMainOn();  ui->_mainOn = true;                         break;
        case 2: ui->_dccex.powerMainOff(); ui->_mainOn = false;                        break;
        case 3: ui->_dccex.powerProgOn();  ui->_progOn = true;                         break;
        case 4: ui->_dccex.powerProgOff(); ui->_progOn = false;                        break;
        case 5: ui->_dccex.powerOn();      ui->_mainOn = true;  ui->_progOn = true;    break;
        case 6: ui->_dccex.powerOff();     ui->_mainOn = false; ui->_progOn = false;   break;
        case 7:
            if (ui->_joinOn) {
                ui->_dccex.powerOff();
                ui->_joinOn = false;
                ui->_mainOn = false;
                ui->_progOn = false;
            } else {
                ui->_dccex.joinProg();
                ui->_joinOn = true;
            }
            break;
    }
    ui->updateStyles();
}
