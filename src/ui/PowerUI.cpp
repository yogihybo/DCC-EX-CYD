#include "PowerUI.h"

static lv_obj_t* make_card(lv_obj_t* parent) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, LV_PCT(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(card, 6, 0);
    lv_obj_set_style_pad_row(card, 5, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1e1e1e), 0);
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
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x555555), 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    return dot;
}

static lv_obj_t* make_action_btn(lv_obj_t* parent, const char* label, int action_id, lv_event_cb_t cb, void* user_data) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, 32);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2e2e2e), 0);
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
    lv_obj_set_height(_btn_join, 32);
    lv_obj_set_style_bg_color(_btn_join, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(_btn_join, 1, 0);
    lv_obj_set_style_border_color(_btn_join, lv_color_hex(0x383838), 0);
    lv_obj_t* join_lbl = lv_label_create(_btn_join);
    lv_label_set_text(join_lbl, "Join tracks");
    lv_obj_set_style_text_color(join_lbl, lv_color_hex(0x777777), 0);
    lv_obj_center(join_lbl);
    lv_obj_set_user_data(_btn_join, (void*)7);
    lv_obj_add_event_cb(_btn_join, btn_event_cb, LV_EVENT_CLICKED, this);

    updateStyles();
}

PowerUI::~PowerUI() {
    if (_container) lv_obj_del(_container);
}

void PowerUI::updateStyles() {
    lv_obj_set_style_bg_color(_btn_main_on,
        _mainOn ? lv_color_make(40, 140, 40) : lv_color_hex(0x2e2e2e), 0);
    lv_obj_set_style_bg_color(_btn_main_off,
        !_mainOn ? lv_color_make(140, 40, 40) : lv_color_hex(0x2e2e2e), 0);

    lv_obj_set_style_bg_color(_dot_main,
        _mainOn ? lv_color_make(76, 175, 80) : lv_color_make(180, 50, 50), 0);
    lv_label_set_text(_lbl_main_status, _mainOn ? "ON" : "OFF");
    lv_obj_set_style_text_color(_lbl_main_status,
        _mainOn ? lv_color_make(76, 175, 80) : lv_color_hex(0x888888), 0);

    lv_obj_set_style_bg_color(_btn_prog_on,
        _progOn ? lv_color_make(40, 140, 40) : lv_color_hex(0x2e2e2e), 0);
    lv_obj_set_style_bg_color(_btn_prog_off,
        !_progOn ? lv_color_make(140, 40, 40) : lv_color_hex(0x2e2e2e), 0);

    lv_obj_set_style_bg_color(_dot_prog,
        _progOn ? lv_color_make(76, 175, 80) : lv_color_make(180, 50, 50), 0);
    lv_label_set_text(_lbl_prog_status, _progOn ? "ON" : "OFF");
    lv_obj_set_style_text_color(_lbl_prog_status,
        _progOn ? lv_color_make(76, 175, 80) : lv_color_hex(0x888888), 0);

    lv_obj_set_style_bg_color(_btn_all_on,
        (_mainOn && _progOn) ? lv_color_make(40, 140, 40) : lv_color_hex(0x2e2e2e), 0);
    lv_obj_set_style_bg_color(_btn_all_off,
        (!_mainOn && !_progOn) ? lv_color_make(140, 40, 40) : lv_color_hex(0x2e2e2e), 0);

    lv_obj_set_style_bg_color(_btn_join,
        _joinOn ? lv_color_make(180, 120, 30) : lv_color_hex(0x222222), 0);
    lv_obj_t* join_lbl = lv_obj_get_child(_btn_join, 0);
    if (join_lbl) {
        lv_label_set_text(join_lbl, _joinOn ? "Unjoin (cuts power)" : "Join tracks");
        lv_obj_set_style_text_color(join_lbl,
            _joinOn ? lv_color_hex(0xeeeeee) : lv_color_hex(0x777777), 0);
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
