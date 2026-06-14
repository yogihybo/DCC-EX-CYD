#include "WiFiUI.h"

WiFiUI::WiFiUI(lv_obj_t* parent) : server(80) {
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
  lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(_container, 0, 0);
  lv_obj_set_style_border_width(_container, 0, 0);


  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(Settings.AP.SSID.c_str(), Settings.AP.password.c_str());
  dns.start(53, Settings.AP.SSID.c_str(), WiFi.softAPIP());
  server.begin();

  _ipGotHandler = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_labelIP) lv_label_set_text_fmt(_labelIP, "IP: %s", WiFi.localIP().toString().c_str());
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

  _ipDisconnectedHandler = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_labelIP) lv_label_set_text(_labelIP, "IP: Not Connected");
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  _updatedHandler = Settings.addEventListener(SettingsClass::Event::CS_CHANGE, [this](void*) {
      // Re-read settings into text areas if not focused
  });

  lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
  
  lv_obj_t* title_row = lv_obj_create(_container);
  lv_obj_set_width(title_row, LV_PCT(100));
  lv_obj_set_height(title_row, 40);
  lv_obj_set_style_pad_all(title_row, 0, 0);
  lv_obj_set_style_border_width(title_row, 0, 0);
  lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(title_row);
  lv_label_set_text(title, "WiFi & CS Settings");
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

  lv_obj_t* close_btn = lv_btn_create(title_row);
  lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_make(200, 50, 50), 0);
  lv_obj_t* close_lbl = lv_label_create(close_btn);
  lv_label_set_text(close_lbl, "Back");
  lv_obj_center(close_lbl);
  lv_obj_add_event_cb(close_btn, close_btn_event_cb, LV_EVENT_CLICKED, this);

  auto create_textarea = [this](const char* placeholder, const char* initial_text, int field) {
    lv_obj_t* ta = lv_textarea_create(_container);
    lv_obj_set_width(ta, LV_PCT(100));
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_textarea_set_text(ta, initial_text);
    lv_obj_set_user_data(ta, (void*)(uintptr_t)field);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_ALL, this);
    return ta;
  };

  lv_obj_t* l1 = lv_label_create(_container); lv_label_set_text(l1, "SSID:");
  _textareaSSID = create_textarea("SSID", Settings.CS.SSID().c_str(), 0);
  
  lv_obj_t* l2 = lv_label_create(_container); lv_label_set_text(l2, "Password:");
  _textareaPassword = create_textarea("Password", Settings.CS.password().c_str(), 1);
  lv_textarea_set_password_mode(_textareaPassword, true);
  
  lv_obj_t* l3 = lv_label_create(_container); lv_label_set_text(l3, "Server IP:");
  _textareaServer = create_textarea("Server IP", Settings.CS.server().c_str(), 2);
  
  lv_obj_t* l4 = lv_label_create(_container); lv_label_set_text(l4, "Port:");
  _textareaPort = create_textarea("Port", String(Settings.CS.port()).c_str(), 3);

  _labelIP = lv_label_create(_container);
  lv_label_set_text(_labelIP, WiFi.isConnected() ? String("IP: " + WiFi.localIP().toString()).c_str() : "IP: Not Connected");

  lv_obj_t* ap_lbl = lv_label_create(_container);
  lv_label_set_text_fmt(ap_lbl, "AP: %s\nPw: %s", Settings.AP.SSID.c_str(), Settings.AP.password.c_str());
  lv_obj_set_style_text_align(ap_lbl, LV_TEXT_ALIGN_CENTER, 0);

  _qr = lv_qrcode_create(_container);
  lv_qrcode_set_size(_qr, 100);
  lv_qrcode_set_dark_color(_qr, lv_color_black());
  lv_qrcode_set_light_color(_qr, lv_color_white());
  
  String qr_data = "WIFI:S:" + Settings.AP.SSID + ";T:WPA;P:" + Settings.AP.password + ";;";
  lv_qrcode_update(_qr, qr_data.c_str(), qr_data.length());

  _keyboard = lv_keyboard_create(lv_layer_top());
  lv_obj_add_flag(_keyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(_keyboard, kb_event_cb, LV_EVENT_ALL, this);

  _loop_timer = lv_timer_create(loop_timer_cb, 50, this);
}

WiFiUI::~WiFiUI() {
  lv_timer_del(_loop_timer);
  WiFi.mode(WIFI_STA);
  dns.stop();
  server.end();
  WiFi.removeEvent(_ipGotHandler);
  WiFi.removeEvent(_ipDisconnectedHandler);
  Settings.removeEventListener(SettingsClass::Event::CS_CHANGE, _updatedHandler);
  
  if (_keyboard) lv_obj_del(_keyboard);
  if (_container) lv_obj_del(_container);
}

void WiFiUI::loop_timer_cb(lv_timer_t* timer) {
  WiFiUI* ui = (WiFiUI*)lv_timer_get_user_data(timer);
  ui->dns.processNextRequest();
}

void WiFiUI::ta_event_cb(lv_event_t * e) {
  WiFiUI* ui = (WiFiUI*)lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
  int field = (int)(uintptr_t)lv_obj_get_user_data(ta);

  if (code == LV_EVENT_FOCUSED) {
    lv_keyboard_set_textarea(ui->_keyboard, ta);
    lv_keyboard_set_mode(ui->_keyboard, (field == 3) ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_clear_flag(ui->_keyboard, LV_OBJ_FLAG_HIDDEN);
    
    // Scroll to the focused textarea so it's not hidden by keyboard
    lv_obj_scroll_to_view(ta, LV_ANIM_ON);
  } else if (code == LV_EVENT_DEFOCUSED) {
    lv_keyboard_set_textarea(ui->_keyboard, NULL);
    lv_obj_add_flag(ui->_keyboard, LV_OBJ_FLAG_HIDDEN);
    
    const char* txt = lv_textarea_get_text(ta);
    if (field == 0) Settings.CS.SSID(txt);
    else if (field == 1) Settings.CS.password(txt);
    else if (field == 2) Settings.CS.server(txt);
    else if (field == 3) Settings.CS.port(atoi(txt));
    
    if (Settings.CS.valid()) {
      Settings.save();
      Settings.dispatchEvent(SettingsClass::Event::CS_CHANGE, reinterpret_cast<void*>(1));
    }
  }
}

void WiFiUI::kb_event_cb(lv_event_t * e) {
  WiFiUI* ui = (WiFiUI*)lv_event_get_user_data(e);
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    lv_obj_t* ta = lv_keyboard_get_textarea(ui->_keyboard);
    if (ta) lv_obj_clear_state(ta, LV_STATE_FOCUSED);
  }
}

void WiFiUI::close_btn_event_cb(lv_event_t * e) {
  WiFiUI* ui = (WiFiUI*)lv_event_get_user_data(e);
  lv_obj_add_flag(ui->_container, LV_OBJ_FLAG_HIDDEN);
}
