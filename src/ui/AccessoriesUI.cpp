#include "AccessoriesUI.h"

AccessoriesUI::AccessoriesUI(DCCExCS& dccExCS, lv_obj_t* parent) : _dccExCS(dccExCS) {
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
  lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(_container, 0, 0);
  lv_obj_set_style_border_width(_container, 0, 0);


  lv_obj_t* title = lv_label_create(_container);
  lv_label_set_text(title, "Accessory / Turnout Control");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t* btn_on = lv_btn_create(_container);
  lv_obj_set_size(btn_on, 200, 60);
  lv_obj_align(btn_on, LV_ALIGN_TOP_MID, 0, 50);
  lv_obj_t* lbl_on = lv_label_create(btn_on);
  lv_label_set_text(lbl_on, "Turnout ON");
  lv_obj_center(lbl_on);
  lv_obj_set_user_data(btn_on, (void*)1); // 1 = ON
  lv_obj_add_event_cb(btn_on, btn_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* btn_off = lv_btn_create(_container);
  lv_obj_set_size(btn_off, 200, 60);
  lv_obj_align(btn_off, LV_ALIGN_TOP_MID, 0, 130);
  lv_obj_t* lbl_off = lv_label_create(btn_off);
  lv_label_set_text(lbl_off, "Turnout OFF");
  lv_obj_center(lbl_off);
  lv_obj_set_user_data(btn_off, (void*)0); // 0 = OFF
  lv_obj_add_event_cb(btn_off, btn_event_cb, LV_EVENT_CLICKED, this);

  _keyboard = nullptr;
  _textarea = nullptr;
}

AccessoriesUI::~AccessoriesUI() {
  if (_container) lv_obj_del(_container);
}

void AccessoriesUI::showKeypad(bool state) {
  _pending_state = state;

  if (!_keyboard) {
    _textarea = lv_textarea_create(_container);
    lv_obj_set_size(_textarea, 200, 40);
    lv_obj_align(_textarea, LV_ALIGN_TOP_MID, 0, 10);
    lv_textarea_set_one_line(_textarea, true);
    lv_textarea_set_placeholder_text(_textarea, "Address (1-2044)");
    lv_obj_add_state(_textarea, LV_STATE_FOCUSED);

    _keyboard = lv_keyboard_create(_container);
    lv_keyboard_set_mode(_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(_keyboard, _textarea);
    lv_obj_add_event_cb(_keyboard, kb_event_cb, LV_EVENT_READY, this);
    lv_obj_add_event_cb(_keyboard, kb_event_cb, LV_EVENT_CANCEL, this);
  }
}

void AccessoriesUI::hideKeypad() {
  if (_keyboard) {
    lv_obj_delete_async(_keyboard);
    _keyboard = nullptr;
  }
  if (_textarea) {
    lv_obj_delete_async(_textarea);
    _textarea = nullptr;
  }
}

void AccessoriesUI::btn_event_cb(lv_event_t * e) {
  AccessoriesUI* ui = (AccessoriesUI*)lv_event_get_user_data(e);
  int action = (int)(uintptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
  ui->showKeypad(action == 1);
}

void AccessoriesUI::kb_event_cb(lv_event_t * e) {
  AccessoriesUI* ui = (AccessoriesUI*)lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_READY) {
    const char* txt = lv_textarea_get_text(ui->_textarea);
    if (txt && strlen(txt) > 0) {
      uint16_t addr = atoi(txt);
      if (addr > 0 && addr <= 2044) {
        ui->_dccExCS.accessory(addr, ui->_pending_state);
      }
    }
    ui->hideKeypad();
  } else if (code == LV_EVENT_CANCEL) {
    ui->hideKeypad();
  }
}
