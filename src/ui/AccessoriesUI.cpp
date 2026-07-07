#include "AccessoriesUI.h"
#include "Theme.h"

AccessoriesUI::AccessoriesUI(DCCEXProtocol& dccex, lv_obj_t* parent)
    : _dccex(dccex), _keyboard(nullptr), _lastState(-1), _lastAddr(0), _pendingQueryAddr(0), _recentCount(0) {

    memset(_recents, 0, sizeof(_recents));

    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(_container, us(5), 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_style_bg_opa(_container, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);

    _content = lv_obj_create(_container);
    lv_obj_set_width(_content, LV_PCT(100));
    lv_obj_set_flex_grow(_content, 1);
    lv_obj_set_style_pad_all(_content, us(6), 0);
    lv_obj_set_style_border_width(_content, 0, 0);
    lv_obj_set_flex_flow(_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_content, us(4), 0);
    lv_obj_clear_flag(_content, LV_OBJ_FLAG_SCROLLABLE);

    _buildContent();
}

void AccessoriesUI::_buildContent() {
    lv_obj_clean(_content);
    _keyboard = nullptr;

    // --- Address ---
    lv_obj_t* addr_lbl = lv_label_create(_content);
    lv_label_set_text(addr_lbl, "Turnout address:");
    lv_obj_set_width(addr_lbl, LV_PCT(100));

    _textarea = lv_textarea_create(_content);
    lv_obj_set_width(_textarea, LV_PCT(100));
    lv_textarea_set_one_line(_textarea, true);
    lv_textarea_set_placeholder_text(_textarea, "1 - 2044");
    lv_textarea_set_accepted_chars(_textarea, "0123456789");
    lv_textarea_set_max_length(_textarea, 4);
    lv_obj_add_event_cb(_textarea, textarea_event_cb, LV_EVENT_FOCUSED, this);

    // --- Recents ---
    lv_obj_t* rec_lbl = lv_label_create(_content);
    lv_label_set_text(rec_lbl, "Recent:");
    lv_obj_set_width(rec_lbl, LV_PCT(100));

    _recents_cont = lv_obj_create(_content);
    lv_obj_set_width(_recents_cont, LV_PCT(100));
    lv_obj_set_height(_recents_cont, us(30));
    lv_obj_set_style_pad_all(_recents_cont, 0, 0);
    lv_obj_set_style_border_width(_recents_cont, 0, 0);
    lv_obj_set_style_bg_opa(_recents_cont, 0, 0);
    lv_obj_set_flex_flow(_recents_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(_recents_cont, us(4), 0);
    lv_obj_clear_flag(_recents_cont, LV_OBJ_FLAG_SCROLLABLE);
    rebuildRecents();

    // --- Throw / Close ---
    lv_obj_t* action_cont = lv_obj_create(_content);
    lv_obj_set_width(action_cont, LV_PCT(100));
    lv_obj_set_height(action_cont, us(38));
    lv_obj_set_style_pad_all(action_cont, 0, 0);
    lv_obj_set_style_border_width(action_cont, 0, 0);
    lv_obj_set_style_bg_opa(action_cont, 0, 0);
    lv_obj_set_flex_flow(action_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(action_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(action_cont, LV_OBJ_FLAG_SCROLLABLE);

    _btn_throw = lv_btn_create(action_cont);
    lv_obj_set_flex_grow(_btn_throw, 1);
    lv_obj_set_height(_btn_throw, us(35));
    lv_obj_set_style_shadow_width(_btn_throw, 0, 0);
    lv_obj_t* throw_lbl = lv_label_create(_btn_throw);
    lv_label_set_text(throw_lbl, "Throw");
    lv_obj_center(throw_lbl);
    lv_obj_set_user_data(_btn_throw, (void*)1);
    lv_obj_add_event_cb(_btn_throw, action_event_cb, LV_EVENT_CLICKED, this);

    _btn_close = lv_btn_create(action_cont);
    lv_obj_set_flex_grow(_btn_close, 1);
    lv_obj_set_height(_btn_close, us(35));
    lv_obj_set_style_shadow_width(_btn_close, 0, 0);
    lv_obj_set_style_bg_color(_btn_close, lv_color_make(40, 140, 40), 0);
    lv_obj_t* close_lbl = lv_label_create(_btn_close);
    lv_label_set_text(close_lbl, "Close");
    lv_obj_center(close_lbl);
    lv_obj_set_user_data(_btn_close, (void*)0);
    lv_obj_add_event_cb(_btn_close, action_event_cb, LV_EVENT_CLICKED, this);

    updateButtonStyles();

    // --- Status ---
    _status_lbl = lv_label_create(_content);
    lv_label_set_text(_status_lbl, "--");
    lv_obj_set_style_text_font(_status_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_status_lbl, tc(TC_TEXT_HINT), 0);
    lv_obj_set_width(_status_lbl, LV_PCT(100));
    lv_obj_set_style_text_align(_status_lbl, LV_TEXT_ALIGN_CENTER, 0);

    _keyboard = nullptr;
}

AccessoriesUI::~AccessoriesUI() {
    if (_keyboard) lv_obj_del(_keyboard);
    if (_container) lv_obj_del(_container);
}

void AccessoriesUI::sendCommand(bool thrown) {
    const char* txt = lv_textarea_get_text(_textarea);
    if (!txt || strlen(txt) == 0) return;
    uint16_t addr = (uint16_t)atoi(txt);
    if (addr < 1 || addr > 2044) return;

    if (thrown) _dccex.activateLinearAccessory(addr);
    else        _dccex.deactivateLinearAccessory(addr);

    _lastAddr  = addr;
    _lastState = thrown ? 1 : 0;
    addRecent(addr);
    updateStatus(thrown, addr);
    updateButtonStyles();
}

void AccessoriesUI::addRecent(uint16_t addr) {
    for (int i = 0; i < _recentCount; i++) {
        if (_recents[i] == addr) {
            for (int j = i; j < _recentCount - 1; j++) _recents[j] = _recents[j + 1];
            _recentCount--;
            break;
        }
    }
    if (_recentCount == MAX_RECENTS) _recentCount--;
    for (int i = _recentCount; i > 0; i--) _recents[i] = _recents[i - 1];
    _recents[0] = addr;
    _recentCount++;
    rebuildRecents();
}

void AccessoriesUI::rebuildRecents() {
    lv_obj_clean(_recents_cont);
    if (_recentCount == 0) {
        lv_obj_t* lbl = lv_label_create(_recents_cont);
        lv_label_set_text(lbl, "No recent addresses");
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, tc(TC_TEXT_MUTED), 0);
        return;
    }
    for (int i = 0; i < _recentCount; i++) {
        lv_obj_t* chip = lv_btn_create(_recents_cont);
        lv_obj_set_size(chip, LV_SIZE_CONTENT, us(28));
        lv_obj_set_style_pad_hor(chip, us(10), 0);
        lv_obj_set_style_pad_ver(chip, 0, 0);
        lv_obj_set_style_bg_color(chip, tc(TC_SURFACE_RAISED), 0);
        lv_obj_set_style_shadow_width(chip, 0, 0);
        lv_obj_set_style_bg_color(chip, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_PRESSED);
        lv_obj_t* lbl = lv_label_create(chip);
        lv_label_set_text_fmt(lbl, "%d", _recents[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
        lv_obj_set_user_data(chip, (void*)(uintptr_t)_recents[i]);
        lv_obj_add_event_cb(chip, chip_event_cb, LV_EVENT_CLICKED, this);
    }
}

void AccessoriesUI::updateStatus(bool thrown, uint16_t addr) {
    if (!_status_lbl) return;
    lv_label_set_text_fmt(_status_lbl, "%s  #%d", thrown ? "Thrown" : "Closed", addr);
    lv_obj_set_style_text_color(_status_lbl,
        thrown ? lv_color_make(180, 120, 30) : lv_color_make(40, 140, 40), 0);
}

void AccessoriesUI::updateButtonStyles() {
    bool thrown = (_lastState == 1);
    bool closed = (_lastState == 0);

    lv_obj_set_style_bg_color(_btn_throw,
        thrown ? lv_color_make(180, 120, 30) : tc(TC_SURFACE_RAISED), 0);
    lv_obj_set_style_bg_color(_btn_close,
        closed ? lv_color_make(40, 140, 40) : tc(TC_SURFACE_RAISED), 0);
}

void AccessoriesUI::queryStatus(uint16_t addr) {
    _pendingQueryAddr = addr;
    _dccex.refreshTurnoutList();
}

void AccessoriesUI::receivedTurnoutList() {
    if (_pendingQueryAddr == 0) return;
    Turnout* t = _dccex.getTurnoutById(_pendingQueryAddr);
    if (!t) {
        if (_status_lbl) {
            lv_label_set_text_fmt(_status_lbl, "ID %d not defined", _pendingQueryAddr);
            lv_obj_set_style_text_color(_status_lbl, tc(TC_TEXT_HINT), 0);
        }
        return;
    }
    bool thrown = t->getThrown();
    _lastState = thrown ? 1 : 0;
    _lastAddr  = _pendingQueryAddr;
    updateButtonStyles();
    updateStatus(thrown, _pendingQueryAddr);
}

void AccessoriesUI::textarea_event_cb(lv_event_t* e) {
    AccessoriesUI* ui = (AccessoriesUI*)lv_event_get_user_data(e);
    if (ui->_keyboard) return;
    ui->_keyboard = lv_keyboard_create(lv_layer_top());
    lv_keyboard_set_mode(ui->_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(ui->_keyboard, ui->_textarea);
    lv_obj_add_event_cb(ui->_keyboard, kb_event_cb, LV_EVENT_READY, ui);
    lv_obj_add_event_cb(ui->_keyboard, kb_event_cb, LV_EVENT_CANCEL, ui);
}

void AccessoriesUI::kb_event_cb(lv_event_t* e) {
    AccessoriesUI* ui = (AccessoriesUI*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY && ui->_textarea) {
        const char* txt = lv_textarea_get_text(ui->_textarea);
        if (txt && strlen(txt) > 0) {
            uint16_t addr = (uint16_t)atoi(txt);
            if (addr >= 1 && addr <= 2044) {
                ui->_lastState = -1;
                ui->updateButtonStyles();
                ui->queryStatus(addr);
            }
        }
    }

    if (ui->_keyboard) {
        lv_obj_delete_async(ui->_keyboard);
        ui->_keyboard = nullptr;
    }
    lv_obj_clear_state(ui->_textarea, LV_STATE_FOCUSED);
}

void AccessoriesUI::action_event_cb(lv_event_t* e) {
    AccessoriesUI* ui = (AccessoriesUI*)lv_event_get_user_data(e);
    int action = (int)(uintptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_current_target(e));
    ui->sendCommand(action == 1);
}

void AccessoriesUI::chip_event_cb(lv_event_t* e) {
    AccessoriesUI* ui = (AccessoriesUI*)lv_event_get_user_data(e);
    uint16_t addr = (uint16_t)(uintptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_current_target(e));
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", addr);
    lv_textarea_set_text(ui->_textarea, buf);
    ui->_lastState = -1;
    ui->updateButtonStyles();
    ui->queryStatus(addr);
}

void AccessoriesUI::receivedTurnoutAction(int id, bool thrown) {
    if (id != _pendingQueryAddr) return;
    _lastState = thrown ? 1 : 0;
    _lastAddr  = (uint16_t)id;
    updateButtonStyles();
    if (_status_lbl)
        updateStatus(thrown, (uint16_t)id);
}
