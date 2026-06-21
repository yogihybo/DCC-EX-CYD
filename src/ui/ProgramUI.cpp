#include "ProgramUI.h"

static void style_popup(lv_obj_t* mbox) {
    lv_obj_set_width(mbox, LV_PCT(90));
    lv_obj_set_style_bg_color(mbox, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_color(mbox, lv_color_hex(0x383838), 0);
    lv_obj_set_style_border_width(mbox, 1, 0);
}

static lv_obj_t* style_popup_title(lv_obj_t* mbox, const char* text, lv_color_t color) {
    lv_obj_t* t = lv_msgbox_add_title(mbox, text);
    lv_obj_set_style_text_color(t, color, 0);
    return t;
}

static lv_obj_t* make_popup_btn(lv_obj_t* mbox, const char* label, lv_color_t bg) {
    lv_obj_t* btn = lv_msgbox_add_footer_button(mbox, label);
    lv_obj_set_style_bg_color(btn, bg, 0);
    return btn;
}

ProgramUI::ProgramUI(DCCEXProtocol& dccex, lv_obj_t* parent)
    : _dccex(dccex), _msgbox(nullptr), _keyboard(nullptr), _ta(nullptr) {

    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(_container, 5, 0);
    lv_obj_set_style_pad_hor(_container, 11, 0);
    lv_obj_set_style_pad_row(_container, 3, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // --- Title row ---
    lv_obj_t* title_row = lv_obj_create(_container);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, 36);
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Program CVs");
    lv_obj_set_style_text_color(title, lv_color_make(38, 166, 154), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* back_btn = lv_btn_create(title_row);
    lv_obj_set_size(back_btn, LV_SIZE_CONTENT, 28);
    lv_obj_set_style_pad_hor(back_btn, 10, 0);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x2e2e2e), 0);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_t* back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, "Back");
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(back_btn, close_btn_event_cb, LV_EVENT_CLICKED, this);

    // --- Helpers ---
    auto add_section = [&](const char* text) {
        lv_obj_t* lbl = lv_label_create(_container);
        lv_label_set_text(lbl, text);
        lv_obj_set_style_text_color(lbl, lv_color_make(38, 166, 154), 0);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_obj_set_style_pad_top(lbl, 4, 0);
    };

    auto make_row = [&](int height) -> lv_obj_t* {
        lv_obj_t* row = lv_obj_create(_container);
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
    };

    auto make_btn = [&](lv_obj_t* row, const char* label, int id) {
        lv_obj_t* btn = lv_btn_create(row);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, 32);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2e2e2e), 0);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_center(lbl);
        lv_obj_set_user_data(btn, (void*)(uintptr_t)id);
        lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, this);
    };

    // --- Address section ---
    add_section("Address");
    lv_obj_t* addr_row = make_row(32);
    make_btn(addr_row, "Read", 0);
    make_btn(addr_row, "Write", 1);

    // --- CV Operations section ---
    add_section("CV Operations");

    // Common CV preset chips
    struct Preset { const char* name; uint16_t cv; };
    const Preset presets[] = {
        {"Address", 1}, {"Accel", 3}, {"Decel", 4}, {"V-Max", 5}, {"Steps", 29}
    };

    lv_obj_t* chips_row = make_row(26);
    for (const auto& p : presets) {
        lv_obj_t* chip = lv_btn_create(chips_row);
        lv_obj_set_flex_grow(chip, 1);
        lv_obj_set_height(chip, 26);
        lv_obj_set_style_pad_hor(chip, 2, 0);
        lv_obj_set_style_pad_ver(chip, 0, 0);
        lv_obj_set_style_bg_color(chip, lv_color_hex(0x1a3a3a), 0);
        lv_obj_t* chip_lbl = lv_label_create(chip);
        lv_label_set_text(chip_lbl, p.name);
        lv_obj_set_style_text_font(chip_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(chip_lbl, lv_color_make(38, 166, 154), 0);
        lv_obj_center(chip_lbl);
        lv_obj_set_user_data(chip, (void*)(uintptr_t)p.cv);
        lv_obj_add_event_cb(chip, preset_cv_event_cb, LV_EVENT_CLICKED, this);
    }

    lv_obj_t* byte_row = make_row(32);
    make_btn(byte_row, "Read Byte", 2);
    make_btn(byte_row, "Write Byte", 3);

    lv_obj_t* bit_row = make_row(32);
    make_btn(bit_row, "Read Bit", 4);
    make_btn(bit_row, "Write Bit", 5);

    // --- Advanced section (ACK) ---
    add_section("Advanced");

    lv_obj_t* ack_row = make_row(28);
    for (int i = 0; i < 3; i++) {
        const char* labels[] = { "ACK Limit", "ACK Min", "ACK Max" };
        lv_obj_t* btn = lv_btn_create(ack_row);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_height(btn, 28);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x383838), 0);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, labels[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x777777), 0);
        lv_obj_center(lbl);
        lv_obj_set_user_data(btn, (void*)(uintptr_t)(6 + i));
        lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, this);
    }
}

ProgramUI::~ProgramUI() {
    cancelTimeout();
    if (_container) lv_obj_del(_container);
}

void ProgramUI::startTimeout() {
    cancelTimeout();
    _timeoutTimer = lv_timer_create(timeout_timer_cb, 10000, this);
    lv_timer_set_repeat_count(_timeoutTimer, 1);
}

void ProgramUI::cancelTimeout() {
    if (_timeoutTimer) {
        lv_timer_del(_timeoutTimer);
        _timeoutTimer = nullptr;
    }
}

void ProgramUI::timeout_timer_cb(lv_timer_t* timer) {
    ProgramUI* ui = (ProgramUI*)lv_timer_get_user_data(timer);
    ui->_timeoutTimer = nullptr;
    ui->result("Timeout", lv_color_make(200, 50, 50));
}

void ProgramUI::receivedReadLoco(int address) {
    cancelTimeout();
    char buf[32]; snprintf(buf, sizeof(buf), "Address: %d", address);
    result(buf, lv_color_make(76, 175, 80));
}

void ProgramUI::receivedWriteLoco(int address) {
    cancelTimeout();
    result("Written OK", lv_color_make(76, 175, 80));
}

void ProgramUI::receivedReadCV(int cv, int value) {
    cancelTimeout();
    if (value == -1) { result("No ACK — check loco is on prog track", lv_color_make(200, 50, 50)); return; }

    if (_step == Step::READ_CV_BIT_PENDING) {
        int bitVal = (value >> _stepData[1]) & 1;
        char buf[48]; snprintf(buf, sizeof(buf), "CV %d bit %d = %d", _stepData[0], _stepData[1], bitVal);
        result(buf, lv_color_make(76, 175, 80));
    } else {
        resultWithWriteBack(cv, value);
    }
}

void ProgramUI::receivedWriteCV(int cv, int value) {
    cancelTimeout();
    if (value == -1) result("No ACK — write failed", lv_color_make(200, 50, 50));
    else { char buf[32]; snprintf(buf, sizeof(buf), "CV %d written OK", cv); result(buf, lv_color_make(76, 175, 80)); }
}

void ProgramUI::close_btn_event_cb(lv_event_t* e) {
    ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
    lv_obj_add_flag(ui->_container, LV_OBJ_FLAG_HIDDEN);
}

void ProgramUI::clearMsgBox() {
    cancelTimeout();
    if (_keyboard) { lv_obj_del(_keyboard); _keyboard = nullptr; }
    if (_msgbox)   { lv_obj_del(_msgbox);   _msgbox = nullptr; _ta = nullptr; }
}

void ProgramUI::msgbox_delete_cb(lv_event_t* e) {
    ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    if (ui->_keyboard) { lv_obj_del(ui->_keyboard); ui->_keyboard = nullptr; }
    if (ui->_msgbox == target) { ui->_msgbox = nullptr; ui->_ta = nullptr; }
}

void ProgramUI::msgbox_close_cb(lv_event_t* e) {
    ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
    ui->clearMsgBox();
}

void ProgramUI::menu_btn_event_cb(lv_event_t* e) {
    ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
    int id = (int)(uintptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_current_target(e));

    switch (id) {
        case 0:
            ui->working();
            ui->startTimeout();
            ui->_dccex.readLoco();
            break;
        case 1: ui->newStep(Step::WRITE_ADDRESS_GET_ADDRESS, "Enter address", 10293, 1); break;
        case 2: ui->newStep(Step::READ_CV_BYTE_GET_CV,       "Read CV — enter number", 1024, 1); break;
        case 3: ui->newStep(Step::WRITE_CV_BYTE_GET_CV,      "Write CV — enter number", 1024, 1); break;
        case 4: ui->newStep(Step::READ_CV_BIT_GET_CV,        "Read Bit — enter CV number", 1024, 1); break;
        case 5: ui->newStep(Step::WRITE_CV_BIT_GET_CV,       "Write Bit — enter CV number", 1024, 1); break;
        case 6: ui->newStep(Step::ACK_LIMIT, "ACK Limit", 100, 30); break;
        case 7: ui->newStep(Step::ACK_MIN,   "ACK Min",   10000, 3000); break;
        case 8: ui->newStep(Step::ACK_MAX,   "ACK Max",   10000, 3000); break;
    }
}

void ProgramUI::preset_cv_event_cb(lv_event_t* e) {
    ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
    uint16_t cv = (uint16_t)(uintptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_current_target(e));
    ui->_stepData[0] = cv;
    ui->_step = Step::READ_CV_BYTE_GET_CV;
    ui->working();
    ui->startTimeout();
    ui->_dccex.readCV(cv);
}

void ProgramUI::newStep(Step step, const char* title, uint16_t max, uint16_t min) {
    _step = step;
    clearMsgBox();

    _msgbox = lv_msgbox_create(_container);
    style_popup(_msgbox);
    lv_obj_set_width(_msgbox, LV_PCT(100));
    lv_obj_align(_msgbox, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(_msgbox, msgbox_delete_cb, LV_EVENT_DELETE, this);
    style_popup_title(_msgbox, title, lv_color_make(38, 166, 154));

    _ta = lv_textarea_create(_msgbox);
    lv_obj_set_width(_ta, LV_PCT(100));
    lv_textarea_set_one_line(_ta, true);
    lv_textarea_set_accepted_chars(_ta, "0123456789");
    lv_textarea_set_max_length(_ta, 5);

    lv_obj_t* cancel_btn = lv_msgbox_add_footer_button(_msgbox, "Cancel");
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0x2e2e2e), 0);
    lv_obj_add_event_cb(cancel_btn, msgbox_close_cb, LV_EVENT_CLICKED, this);

    _keyboard = lv_keyboard_create(_container);
    lv_keyboard_set_mode(_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(_keyboard, _ta);
    lv_obj_add_event_cb(_keyboard, keypad_event_cb, LV_EVENT_READY, this);
    lv_obj_add_event_cb(_keyboard, keypad_event_cb, LV_EVENT_CANCEL, this);
}

void ProgramUI::keypad_event_cb(lv_event_t* e) {
    ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        if (ui->_ta) ui->keypadEnter(atoi(lv_textarea_get_text(ui->_ta)));
    } else if (code == LV_EVENT_CANCEL) {
        ui->clearMsgBox();
    }
}

void ProgramUI::working() {
    clearMsgBox();
    _msgbox = lv_msgbox_create(_container);
    style_popup(_msgbox);
    lv_obj_set_width(_msgbox, LV_PCT(100));
    lv_obj_align(_msgbox, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(_msgbox, msgbox_delete_cb, LV_EVENT_DELETE, this);
    style_popup_title(_msgbox, "Working...", lv_color_hex(0x888888));
}

void ProgramUI::result(const char* message, lv_color_t color) {
    clearMsgBox();
    _msgbox = lv_msgbox_create(_container);
    style_popup(_msgbox);
    lv_obj_set_width(_msgbox, LV_PCT(100));
    lv_obj_align(_msgbox, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(_msgbox, msgbox_delete_cb, LV_EVENT_DELETE, this);
    style_popup_title(_msgbox, message, color);
    lv_obj_t* ok = make_popup_btn(_msgbox, "OK", lv_color_hex(0x2e2e2e));
    lv_obj_add_event_cb(ok, msgbox_close_cb, LV_EVENT_CLICKED, this);
}

void ProgramUI::resultWithWriteBack(int cv, int value) {
    clearMsgBox();
    _msgbox = lv_msgbox_create(_container);
    style_popup(_msgbox);
    lv_obj_set_width(_msgbox, LV_PCT(100));
    lv_obj_align(_msgbox, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(_msgbox, msgbox_delete_cb, LV_EVENT_DELETE, this);

    char title[32]; snprintf(title, sizeof(title), "CV %d  =  %d", cv, value);
    style_popup_title(_msgbox, title, lv_color_make(76, 175, 80));

    lv_obj_t* write_btn = make_popup_btn(_msgbox, "Write new value", lv_color_hex(0x1a3a3a));
    lv_obj_set_style_text_color(lv_obj_get_child(write_btn, 0), lv_color_make(38, 166, 154), 0);
    lv_obj_add_event_cb(write_btn, write_back_btn_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* ok = make_popup_btn(_msgbox, "OK", lv_color_hex(0x2e2e2e));
    lv_obj_add_event_cb(ok, msgbox_close_cb, LV_EVENT_CLICKED, this);
}

void ProgramUI::write_back_btn_cb(lv_event_t* e) {
    ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
    char title[48]; snprintf(title, sizeof(title), "CV %d \xe2\x80\x94 enter new value", ui->_stepData[0]);
    ui->newStep(Step::WRITE_CV_BYTE_WRITEBACK, title, 255, 0);
}

void ProgramUI::confirm(const char* message) {
    clearMsgBox();
    _msgbox = lv_msgbox_create(_container);
    style_popup(_msgbox);
    lv_obj_set_width(_msgbox, LV_PCT(100));
    lv_obj_align(_msgbox, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_event_cb(_msgbox, msgbox_delete_cb, LV_EVENT_DELETE, this);
    style_popup_title(_msgbox, "Confirm", lv_color_make(38, 166, 154));

    lv_obj_t* txt = lv_msgbox_add_text(_msgbox, message);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(txt, lv_color_hex(0xaaaaaa), 0);

    lv_obj_t* yes_btn = make_popup_btn(_msgbox, "Write", lv_color_make(40, 140, 40));
    lv_obj_add_event_cb(yes_btn, confirm_btn_event_cb, LV_EVENT_CLICKED, this);

    lv_obj_t* no_btn = make_popup_btn(_msgbox, "Cancel", lv_color_hex(0x2e2e2e));
    lv_obj_add_event_cb(no_btn, msgbox_close_cb, LV_EVENT_CLICKED, this);
}

void ProgramUI::confirm_btn_event_cb(lv_event_t* e) {
    ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
    uint32_t number = ui->_stepData[2];

    switch (ui->_step) {
        case Step::WRITE_ADDRESS_GET_ADDRESS:
            ui->working(); ui->startTimeout();
            ui->_dccex.writeLocoAddress(number);
            break;
        case Step::WRITE_CV_BYTE_GET_VALUE:
        case Step::WRITE_CV_BYTE_WRITEBACK:
            ui->working(); ui->startTimeout();
            ui->_dccex.writeCV(ui->_stepData[0], number);
            break;
        case Step::WRITE_CV_BIT_GET_VALUE:
            ui->working(); ui->startTimeout();
            ui->_dccex.writeCVBit(ui->_stepData[0], ui->_stepData[1], number);
            break;
        default: break;
    }
}

void ProgramUI::keypadEnter(uint32_t number) {
    switch (_step) {
        case Step::WRITE_ADDRESS_GET_ADDRESS:
            _stepData[2] = number;
            { char buf[32]; snprintf(buf, sizeof(buf), "Address: %lu", number); confirm(buf); }
            break;

        case Step::READ_CV_BYTE_GET_CV:
            _stepData[0] = number;
            working(); startTimeout();
            _dccex.readCV(number);
            break;

        case Step::WRITE_CV_BYTE_GET_CV:
            _stepData[0] = number;
            { char buf[48]; snprintf(buf, sizeof(buf), "CV %lu \xe2\x80\x94 enter value", number);
              newStep(Step::WRITE_CV_BYTE_GET_VALUE, buf, 255, 0); }
            break;

        case Step::WRITE_CV_BYTE_GET_VALUE:
        case Step::WRITE_CV_BYTE_WRITEBACK:
            _stepData[2] = number;
            { char buf[32]; snprintf(buf, sizeof(buf), "CV %d  =  %lu", _stepData[0], number); confirm(buf); }
            break;

        case Step::READ_CV_BIT_GET_CV:
            _stepData[0] = number;
            { char buf[48]; snprintf(buf, sizeof(buf), "CV %lu \xe2\x80\x94 enter bit (0-7)", number);
              newStep(Step::READ_CV_BIT_GET_BIT, buf, 7, 0); }
            break;

        case Step::READ_CV_BIT_GET_BIT:
            _stepData[1] = number;
            _step = Step::READ_CV_BIT_PENDING;
            working(); startTimeout();
            _dccex.readCV(_stepData[0]);
            break;

        case Step::WRITE_CV_BIT_GET_CV:
            _stepData[0] = number;
            { char buf[48]; snprintf(buf, sizeof(buf), "CV %lu \xe2\x80\x94 enter bit (0-7)", number);
              newStep(Step::WRITE_CV_BIT_GET_BIT, buf, 7, 0); }
            break;

        case Step::WRITE_CV_BIT_GET_BIT:
            _stepData[1] = number;
            { char buf[56]; snprintf(buf, sizeof(buf), "CV %d bit %lu \xe2\x80\x94 enter value (0-1)", _stepData[0], number);
              newStep(Step::WRITE_CV_BIT_GET_VALUE, buf, 1, 0); }
            break;

        case Step::WRITE_CV_BIT_GET_VALUE:
            _stepData[2] = number;
            { char buf[48]; snprintf(buf, sizeof(buf), "CV %d bit %d = %lu", _stepData[0], _stepData[1], number); confirm(buf); }
            break;

        case Step::ACK_LIMIT: {
            char cmd[32]; snprintf(cmd, sizeof(cmd), "D ACK LIMIT %lu", number);
            _dccex.sendCommand(cmd);
            { char buf[32]; snprintf(buf, sizeof(buf), "ACK Limit set to %lu", number); result(buf, lv_color_make(76, 175, 80)); }
        } break;

        case Step::ACK_MIN: {
            char cmd[32]; snprintf(cmd, sizeof(cmd), "D ACK MIN %lu", number);
            _dccex.sendCommand(cmd);
            { char buf[32]; snprintf(buf, sizeof(buf), "ACK Min set to %lu", number); result(buf, lv_color_make(76, 175, 80)); }
        } break;

        case Step::ACK_MAX: {
            char cmd[32]; snprintf(cmd, sizeof(cmd), "D ACK MAX %lu", number);
            _dccex.sendCommand(cmd);
            { char buf[32]; snprintf(buf, sizeof(buf), "ACK Max set to %lu", number); result(buf, lv_color_make(76, 175, 80)); }
        } break;

        default: break;
    }
}
