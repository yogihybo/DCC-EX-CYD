#include "SettingsUI.h"
#include "WiFiUI.h"
#include "AboutUI.h"

SettingsUI::SettingsUI(DCCExCS& dccExCS, lv_obj_t* parent) : _dccExCS(dccExCS), _wifiUI(nullptr), _aboutUI(nullptr) {
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
  lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(_container, 0, 0);
  lv_obj_set_style_border_width(_container, 0, 0);

  _tabview = lv_tabview_create(_container);
  lv_tabview_set_tab_bar_position(_tabview, LV_DIR_TOP);
  lv_tabview_set_tab_bar_size(_tabview, 40);

  lv_obj_t* tab1 = lv_tabview_add_tab(_tabview, "Loco");
  lv_obj_t* tab2 = lv_tabview_add_tab(_tabview, "System");
  lv_obj_t* tab3 = lv_tabview_add_tab(_tabview, "Conn");

  // Tab 1: Loco Options
  lv_obj_set_flex_flow(tab1, LV_FLEX_FLOW_COLUMN);
  
  lv_obj_t* speed_btn = lv_btn_create(tab1);
  lv_obj_set_width(speed_btn, LV_PCT(100));
  _speedStepLbl = lv_label_create(speed_btn);
  lv_label_set_text_fmt(_speedStepLbl, "Speed Step: %d", Settings.LocoUI.speedStep);
  lv_obj_center(_speedStepLbl);
  lv_obj_add_event_cb(speed_btn, speed_step_event_cb, LV_EVENT_CLICKED, this);

  // Tab 2: System
  lv_obj_set_flex_flow(tab2, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* rotation_btn = lv_btn_create(tab2);
  lv_obj_set_width(rotation_btn, LV_PCT(100));
  _rotationLbl = lv_label_create(rotation_btn);
  lv_label_set_text_fmt(_rotationLbl, "Rotation: %d", Settings.rotation);
  lv_obj_center(_rotationLbl);
  lv_obj_add_event_cb(rotation_btn, rotation_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* storage_btn = lv_btn_create(tab2);
  lv_obj_set_width(storage_btn, LV_PCT(100));
  _storageModeLbl = lv_label_create(storage_btn);
  lv_label_set_text_fmt(_storageModeLbl, "Storage: %s", Settings.storageMode == SettingsClass::StorageMode::SD_CARD ? "SD Card" : "Internal");
  lv_obj_center(_storageModeLbl);
  lv_obj_add_event_cb(storage_btn, storage_mode_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* br_cont = lv_obj_create(tab2);
  lv_obj_set_width(br_cont, LV_PCT(100));
  lv_obj_set_height(br_cont, 80);
  _brightnessLbl = lv_label_create(br_cont);
  lv_label_set_text_fmt(_brightnessLbl, "Brightness: %d%%", (Settings.brightness * 100) / 255);
  lv_obj_align(_brightnessLbl, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t* br_slider = lv_slider_create(br_cont);
  lv_obj_set_width(br_slider, 180);
  lv_obj_align(br_slider, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_slider_set_range(br_slider, 10, 255);
  lv_slider_set_value(br_slider, Settings.brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(br_slider, brightness_event_cb, LV_EVENT_VALUE_CHANGED, this);

  _pinBtn = lv_btn_create(tab2);
  lv_obj_set_width(_pinBtn, LV_PCT(100));
  lv_obj_t* pin_lbl = lv_label_create(_pinBtn);
  lv_label_set_text(pin_lbl, Settings.pin == 0 ? "Pin: Not Set" : "Pin: Set");
  lv_obj_center(pin_lbl);

  // Tab 3: Connections
  lv_obj_set_flex_flow(tab3, LV_FLEX_FLOW_COLUMN);

  lv_obj_t* wifi_btn = lv_btn_create(tab3);
  lv_obj_set_width(wifi_btn, LV_PCT(100));
  lv_obj_t* wifi_lbl = lv_label_create(wifi_btn);
  lv_label_set_text(wifi_lbl, "WiFi Setup");
  lv_obj_center(wifi_lbl);
  lv_obj_add_event_cb(wifi_btn, wifi_setup_event_cb, LV_EVENT_CLICKED, this);

  _aboutUI = new AboutUI(_dccExCS, tab3);
}

SettingsUI::~SettingsUI() {
  Settings.save();
  if (_wifiUI) delete _wifiUI;
  if (_aboutUI) delete _aboutUI;
  if (_container) lv_obj_del(_container);
}

void SettingsUI::speed_step_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (++Settings.LocoUI.speedStep > 2) Settings.LocoUI.speedStep = 0;
  lv_label_set_text_fmt(ui->_speedStepLbl, "Speed Step: %d", Settings.LocoUI.speedStep);
}

void SettingsUI::rotation_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (++Settings.rotation > 2) Settings.rotation = 0;
  lv_label_set_text_fmt(ui->_rotationLbl, "Rotation: %d", Settings.rotation);
  Settings.dispatchEvent(SettingsClass::Event::ROTATION_CHANGE);
}

void SettingsUI::storage_mode_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  Settings.storageMode = (Settings.storageMode == SettingsClass::StorageMode::SPIFFS) ? SettingsClass::StorageMode::SD_CARD : SettingsClass::StorageMode::SPIFFS;
  lv_label_set_text_fmt(ui->_storageModeLbl, "Storage: %s", Settings.storageMode == SettingsClass::StorageMode::SD_CARD ? "SD Card" : "Internal");
}

void SettingsUI::brightness_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
  Settings.brightness = lv_slider_get_value(slider);
  lv_label_set_text_fmt(ui->_brightnessLbl, "Brightness: %d%%", (Settings.brightness * 100) / 255);
  Settings.dispatchEvent(SettingsClass::Event::BRIGHTNESS_CHANGE);
}

void SettingsUI::wifi_setup_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (!ui->_wifiUI) {
      ui->_wifiUI = new WiFiUI(ui->_container);
  } else {
      lv_obj_clear_flag(ui->_wifiUI->getContainer(), LV_OBJ_FLAG_HIDDEN);
  }
}

void SettingsUI::about_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (ui->_aboutUI) {
      delete ui->_aboutUI;
  }
  // Use lv_layer_top() to guarantee it completely covers the full width of the screen over the header
  ui->_aboutUI = new AboutUI(ui->_dccExCS, lv_layer_top());
}
