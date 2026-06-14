#include "ProgramUI.h"

ProgramUI::ProgramUI(DCCExCS& dccExCS, lv_obj_t* parent) : _dccExCS(dccExCS), _msgbox(nullptr), _keyboard(nullptr), _ta(nullptr) {
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(_container, 0, 0);
  lv_obj_set_style_border_width(_container, 0, 0);
  lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);


  // Top header with Back button
  lv_obj_t* header = lv_obj_create(_container);
  lv_obj_set_width(header, LV_PCT(100));
  lv_obj_set_height(header, 50);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "Program CVs");
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

  lv_obj_t* close_btn = lv_btn_create(header);
  lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_make(200, 50, 50), 0);
  lv_obj_t* close_lbl = lv_label_create(close_btn);
  lv_label_set_text(close_lbl, "Back");
  lv_obj_center(close_lbl);
  lv_obj_add_event_cb(close_btn, close_btn_event_cb, LV_EVENT_CLICKED, this);

  // Content area for buttons
  lv_obj_t* content = lv_obj_create(_container);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_grow(content, 1);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(content, 10, 0);

  const char* btn_names[] = {
    "Read Address", "Write Address",
    "Read Byte", "Write Byte",
    "Read Bit", "Write Bit",
    "ACK Limit", "ACK Min", "ACK Max"
  };

  for(int i = 0; i < 9; i++) {
    lv_obj_t* btn = lv_btn_create(content);
    lv_obj_set_width(btn, 140); // Two columns, auto height
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, btn_names[i]);
    lv_obj_center(lbl);
    lv_obj_set_user_data(btn, (void*)(uintptr_t)i);
    lv_obj_add_event_cb(btn, menu_btn_event_cb, LV_EVENT_CLICKED, this);
  }

  // DCC Event Listeners
  _timeoutHandler = _dccExCS.addEventListener(DCCExCS::Event::TIMEOUT, [this](void*) {
    result("Timeout", lv_color_make(255, 0, 0));
  });
  _writeHandler = _dccExCS.addEventListener(DCCExCS::Event::PROGRAM_WRITE, [this](void* parameter) {
    if (*static_cast<int16_t*>(parameter) == -1) {
      result("Error", lv_color_make(255, 0, 0));
    } else {
      result("Success", lv_color_make(0, 255, 0));
    }
  });
  _readHandler = _dccExCS.addEventListener(DCCExCS::Event::PROGRAM_READ, [this](void* parameter) {
    if (*static_cast<int16_t*>(parameter) == -1) {
      result("Error", lv_color_make(255, 0, 0));
    } else {
      result(String(*static_cast<int16_t*>(parameter)), lv_color_make(0, 255, 0));
    }
  });
}

ProgramUI::~ProgramUI() {
  _dccExCS.removeEventListener(DCCExCS::Event::TIMEOUT, _timeoutHandler);
  _dccExCS.removeEventListener(DCCExCS::Event::PROGRAM_WRITE, _writeHandler);
  _dccExCS.removeEventListener(DCCExCS::Event::PROGRAM_READ, _readHandler);
  if (_container) lv_obj_del(_container);
}

void ProgramUI::close_btn_event_cb(lv_event_t * e) {
  ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
  lv_obj_add_flag(ui->_container, LV_OBJ_FLAG_HIDDEN);
}

void ProgramUI::clearMsgBox() {
  if (_keyboard) {
    lv_obj_del(_keyboard);
    _keyboard = nullptr;
  }
  if (_msgbox) {
    lv_obj_del(_msgbox); 
    _msgbox = nullptr;
    _ta = nullptr;
  }
}

void ProgramUI::msgbox_delete_cb(lv_event_t * e) {
  ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
  lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
  
  if (ui->_keyboard) {
    lv_obj_del(ui->_keyboard);
    ui->_keyboard = nullptr;
  }
  
  if (ui->_msgbox == target) {
    ui->_msgbox = nullptr;
    ui->_ta = nullptr;
  }
}

void ProgramUI::msgbox_close_cb(lv_event_t * e) {
  ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
  ui->clearMsgBox();
}

void ProgramUI::menu_btn_event_cb(lv_event_t * e) {
  ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
  lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
  int id = (int)(uintptr_t)lv_obj_get_user_data(btn);

  switch(id) {
    case 0: // Read Address
      ui->working();
      ui->_dccExCS.getLocoAddress();
      break;
    case 1: ui->newStep(Step::WRITE_ADDRESS_GET_ADDRESS, "Enter Address", 10293, 1); break;
    case 2: ui->newStep(Step::READ_CV_BYTE_GET_CV, "Enter CV Address", 1024, 1); break;
    case 3: ui->newStep(Step::WRITE_CV_BYTE_GET_CV, "Enter CV Address", 1024, 1); break;
    case 4: ui->newStep(Step::READ_CV_BIT_GET_CV, "Enter CV Address", 1024, 1); break;
    case 5: ui->newStep(Step::WRITE_CV_BIT_GET_CV, "Enter CV Address", 1024, 1); break;
    case 6: ui->newStep(Step::ACK_LIMIT, "Enter ACK Limit", 100, 30); break;
    case 7: ui->newStep(Step::ACK_MIN, "Enter ACK Min", 10000, 3000); break;
    case 8: ui->newStep(Step::ACK_MAX, "Enter ACK Max", 10000, 3000); break;
  }
}

void ProgramUI::newStep(Step step, const String& title, uint16_t max, uint16_t min) {
  _step = step;
  clearMsgBox();

  _msgbox = lv_msgbox_create(_container);
  lv_obj_set_width(_msgbox, LV_PCT(100)); // Scale to fit screen width
  lv_obj_align(_msgbox, LV_ALIGN_TOP_MID, 0, 10);
  lv_obj_add_event_cb(_msgbox, msgbox_delete_cb, LV_EVENT_DELETE, this);
  lv_msgbox_add_title(_msgbox, title.c_str());
  
  lv_msgbox_add_close_button(_msgbox);

  _ta = lv_textarea_create(_msgbox);
  lv_textarea_set_one_line(_ta, true);
  lv_textarea_set_accepted_chars(_ta, "0123456789");
  lv_textarea_set_max_length(_ta, 5);

  _keyboard = lv_keyboard_create(_container);
  lv_keyboard_set_mode(_keyboard, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(_keyboard, _ta);
  lv_obj_add_event_cb(_keyboard, keypad_event_cb, LV_EVENT_READY, this);
  lv_obj_add_event_cb(_keyboard, keypad_event_cb, LV_EVENT_CANCEL, this);
}

void ProgramUI::keypad_event_cb(lv_event_t * e) {
  ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);

  if (code == LV_EVENT_READY) {
    if (ui->_ta) {
      const char* txt = lv_textarea_get_text(ui->_ta);
      uint32_t val = atoi(txt);
      ui->keypadEnter(val);
    }
  } else if (code == LV_EVENT_CANCEL) {
    ui->clearMsgBox();
  }
}

void ProgramUI::working() {
  clearMsgBox();
  _msgbox = lv_msgbox_create(_container);
  lv_obj_add_event_cb(_msgbox, msgbox_delete_cb, LV_EVENT_DELETE, this);
  lv_msgbox_add_title(_msgbox, "Working...");
}

void ProgramUI::result(const String& message, lv_color_t color) {
  clearMsgBox();
  _msgbox = lv_msgbox_create(_container);
  lv_obj_add_event_cb(_msgbox, msgbox_delete_cb, LV_EVENT_DELETE, this);
  lv_obj_t* title = lv_msgbox_add_title(_msgbox, message.c_str());
  lv_obj_set_style_text_color(title, color, 0);

  lv_msgbox_add_close_button(_msgbox);
}

void ProgramUI::confirm(const String& message) {
  clearMsgBox();
  _msgbox = lv_msgbox_create(_container);
  lv_obj_add_event_cb(_msgbox, msgbox_delete_cb, LV_EVENT_DELETE, this);
  lv_msgbox_add_title(_msgbox, "Confirm");
  lv_msgbox_add_text(_msgbox, message.c_str());
  
  lv_obj_t* yes_btn = lv_msgbox_add_footer_button(_msgbox, "Yes");
  lv_obj_add_event_cb(yes_btn, confirm_btn_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* no_btn = lv_msgbox_add_footer_button(_msgbox, "No");
  lv_obj_add_event_cb(no_btn, msgbox_close_cb, LV_EVENT_CLICKED, this);
}

void ProgramUI::confirm_btn_event_cb(lv_event_t * e) {
  ProgramUI* ui = (ProgramUI*)lv_event_get_user_data(e);
  uint32_t number = ui->_stepData[2];

  switch(ui->_step) {
    case Step::WRITE_ADDRESS_GET_ADDRESS:
      ui->working();
      ui->_dccExCS.setLocoAddress(number);
      break;
    case Step::WRITE_CV_BYTE_GET_VALUE:
      ui->working();
      ui->_dccExCS.setLocoCVByte(ui->_stepData[0], number);
      break;
    case Step::WRITE_CV_BIT_GET_VALUE:
      ui->working();
      ui->_dccExCS.setLocoCVBit(ui->_stepData[0], ui->_stepData[1], number);
      break;
    default:
      break;
  }
}

void ProgramUI::keypadEnter(uint32_t number) {
  switch (_step) {
    case Step::WRITE_ADDRESS_GET_ADDRESS: {
      _stepData[2] = number;
      String message = "Write Address?\nAddress: " + String(number);
      confirm(message);
    } break;
    case Step::READ_CV_BYTE_GET_CV: {
      working();
      _dccExCS.getLocoCVByte(number);
    } break;
    case Step::WRITE_CV_BYTE_GET_CV: {
      _stepData[0] = number;
      newStep(Step::WRITE_CV_BYTE_GET_VALUE, "Enter Byte Value", 255, 0);
    } break;
    case Step::WRITE_CV_BYTE_GET_VALUE: {
      _stepData[2] = number;
      String message = "Write CV Value?\nCV: " + String(_stepData[0]) + "\nValue: " + String(number);
      confirm(message);
    } break;
    case Step::READ_CV_BIT_GET_CV: {
      _stepData[0] = number;
      newStep(Step::READ_CV_BIT_GET_BIT, "Enter Bit", 7, 0);
    } break;
    case Step::READ_CV_BIT_GET_BIT: {
      working();
      _dccExCS.getLocoCVBit(_stepData[0], number);
    } break;
    case Step::WRITE_CV_BIT_GET_CV: {
      _stepData[0] = number;
      newStep(Step::WRITE_CV_BIT_GET_BIT, "Enter Bit", 7, 0);
    } break;
    case Step::WRITE_CV_BIT_GET_BIT: {
      _stepData[1] = number;
      newStep(Step::WRITE_CV_BIT_GET_VALUE, "Enter Value", 1, 0);
    } break;
    case Step::WRITE_CV_BIT_GET_VALUE: {
      _stepData[2] = number;
      String message = "Write CV Bit Value?\nCV: " + String(_stepData[0]) + "\nBit: " + String(_stepData[1]) + "\nValue: " + String(number);
      confirm(message);
    } break;
    case Step::ACK_LIMIT: {
      _dccExCS.setAckLimit(number);
      result("ACK Set", lv_color_make(0, 255, 0));
    } break;
    case Step::ACK_MIN: {
      _dccExCS.setAckMin(number);
      result("ACK Set", lv_color_make(0, 255, 0));
    } break;
    case Step::ACK_MAX: {
      _dccExCS.setAckMax(number);
      result("ACK Set", lv_color_make(0, 255, 0));
    } break;
  }
}
