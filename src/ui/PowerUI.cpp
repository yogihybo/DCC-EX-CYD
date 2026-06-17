#include "PowerUI.h"

PowerUI::PowerUI(DCCExCS& dccExCS, DCCExCS::Power& power, lv_obj_t* parent)
    : _dccExCS(dccExCS), _power(power) {
  
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
  lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(_container, 0, 0);
  lv_obj_set_style_border_width(_container, 0, 0);

  lv_obj_t* title = lv_label_create(_container);
  lv_label_set_text(title, "Track Power Control");
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  auto create_btn = [this](int x, int y, int w, int h, const char* label, int action_id, bool checked) {
    lv_obj_t* btn = lv_btn_create(_container);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, x, y);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    if (checked) lv_obj_add_state(btn, LV_STATE_CHECKED);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_center(lbl);
    lv_obj_set_user_data(btn, (void*)(uintptr_t)action_id);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_VALUE_CHANGED, this);
    return btn;
  };

  _powerMain = create_btn(-55, 40, 100, 45, "Main", 1, _power.main);
  _powerProg = create_btn(55, 40, 100, 45, "Prog", 2, _power.prog);
  _powerAll = create_btn(0, 95, 210, 45, "All Tracks", 3, _power.main && _power.prog);
  _powerJoin = create_btn(0, 150, 210, 45, "Join Tracks", 4, _power.join);

  _broadcastPowerHandler = _dccExCS.addEventListener(DCCExCS::Event::BROADCAST_POWER, [this](void* parameter) {
    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        auto power = *static_cast<DCCExCS::Power*>(parameter);

        Serial.printf("[DCC] Track power – Main: %s  Prog: %s  Join: %s\n",
            power.main ? "ON" : "OFF",
            power.prog ? "ON" : "OFF",
            power.join ? "ON" : "OFF");

        if (_powerMain) {
          if (power.main) lv_obj_add_state(_powerMain, LV_STATE_CHECKED);
          else lv_obj_clear_state(_powerMain, LV_STATE_CHECKED);
        }

        if (_powerProg) {
          if (power.prog) lv_obj_add_state(_powerProg, LV_STATE_CHECKED);
          else lv_obj_clear_state(_powerProg, LV_STATE_CHECKED);
        }

        if (_powerAll) {
          if (power.main && power.prog) lv_obj_add_state(_powerAll, LV_STATE_CHECKED);
          else lv_obj_clear_state(_powerAll, LV_STATE_CHECKED);
        }

        if (_powerJoin) {
          if (power.join) lv_obj_add_state(_powerJoin, LV_STATE_CHECKED);
          else lv_obj_clear_state(_powerJoin, LV_STATE_CHECKED);
        }
        xSemaphoreGive(lvgl_mutex);
    }
  });
}

PowerUI::~PowerUI() {
  _dccExCS.removeEventListener(DCCExCS::Event::BROADCAST_POWER, _broadcastPowerHandler);
  if (_container) lv_obj_del(_container);
}

void PowerUI::btn_event_cb(lv_event_t * e) {
  PowerUI* ui = (PowerUI*)lv_event_get_user_data(e);
  lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
  int action = (int)(uintptr_t)lv_obj_get_user_data(btn);
  bool state = lv_obj_has_state(btn, LV_STATE_CHECKED);

  if (action == 1) ui->_dccExCS.setCSPower(DCCExCS::Power::MAIN, state);
  else if (action == 2) ui->_dccExCS.setCSPower(DCCExCS::Power::PROG, state);
  else if (action == 3) ui->_dccExCS.setCSPower(DCCExCS::Power::ALL, state);
  else if (action == 4) ui->_dccExCS.setCSPower(DCCExCS::Power::JOIN, state);
}
