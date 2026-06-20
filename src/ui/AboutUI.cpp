#include "AboutUI.h"
#include <Version.h>
#include <WiFi.h>
#include <Settings.h>
#include <SD.h>

// Definition of extern declared in AboutUI.h
CSInfo csInfo;

AboutUI::AboutUI(DCCEXProtocol& dccex, lv_obj_t* parent) {
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
  lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(_container, 0, 0);
  lv_obj_set_style_border_width(_container, 0, 0);
  lv_obj_set_style_bg_opa(_container, LV_OPA_COVER, 0);

  lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(_container, 2, 0);
  lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* header = lv_obj_create(_container);
  lv_obj_set_width(header, LV_PCT(100));
  lv_obj_set_height(header, 50);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(header);
  lv_label_set_text(title, "About");
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 10, 0);

  lv_obj_t* close_btn = lv_btn_create(header);
  lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_make(200, 50, 50), 0);
  lv_obj_t* close_lbl = lv_label_create(close_btn);
  lv_label_set_text(close_lbl, "Back");
  lv_obj_center(close_lbl);
  lv_obj_add_event_cb(close_btn, close_btn_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* content = lv_obj_create(_container);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_grow(content, 1);
  lv_obj_set_style_pad_all(content, 5, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(content, 2, 0);

#if LV_USE_FONT_MONTSERRAT_12
  lv_obj_set_style_text_font(content, &lv_font_montserrat_12, 0);
#endif

  auto add_heading = [&](const char* text) {
      lv_obj_t* h = lv_label_create(content);
      lv_label_set_text(h, text);
      lv_obj_set_style_text_color(h, lv_color_make(52, 204, 211), 0);
      lv_obj_set_style_pad_top(h, 8, 0);
  };

  // --- Throttle ---
  add_heading("Throttle");

  String version("Version: ");
  version += T3_VERSION_MAJOR; version += "."; version += T3_VERSION_MINOR; version += "."; version += T3_VERSION_PATCH;
  lv_obj_t* t3_ver = lv_label_create(content);
  lv_label_set_text(t3_ver, version.c_str());

  lv_obj_t* platform_lbl = lv_label_create(content);
  lv_label_set_text_fmt(platform_lbl, "Platform: ESP32 (%s)", ESP.getChipModel());

  _memLbl = lv_label_create(content);
  lv_label_set_text_fmt(_memLbl, "Free RAM: %d KB", ESP.getFreeHeap() / 1024);

  // --- Connection ---
  add_heading("Connection");

  _wifiStat = lv_label_create(content);
  if (WiFi.status() == WL_CONNECTED) {
      lv_label_set_text_fmt(_wifiStat, "WiFi: Connected\nIP: %s\nmDNS: dcc-ex-cyd.local", WiFi.localIP().toString().c_str());
  } else {
      lv_label_set_text(_wifiStat, "WiFi: Disconnected");
  }

  lv_obj_t* apStat = lv_label_create(content);
  lv_label_set_text_fmt(apStat, "Access Point: %s\nAP Name: %s\nAP Password: %s",
                        Settings.AP.enabled ? "On" : "Off",
                        Settings.AP.SSID.c_str(),
                        Settings.AP.password.c_str());

  // --- SD Card ---
  add_heading("SD Card");

  _sdStat = lv_label_create(content);
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
      lv_label_set_text(_sdStat, "Status: Not Attached");
  } else {
      const char* typeStr = "UNKNOWN";
      if (cardType == CARD_MMC)  typeStr = "MMC";
      else if (cardType == CARD_SD)   typeStr = "SDSC";
      else if (cardType == CARD_SDHC) typeStr = "SDHC";
      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      lv_label_set_text_fmt(_sdStat, "Status: Mounted\nFormat: %s\nCapacity: %llu MB", typeStr, cardSize);
  }

  // --- DCC-EX Command Station ---
  add_heading("DCC-EX Command Station");

  _csConnected = lv_label_create(content);
  lv_label_set_text(_csConnected, csInfo.connected ? "Status: Connected" : "Status: Not Connected");

  _csVersion = lv_label_create(content);
  lv_label_set_text_fmt(_csVersion, "Version: %s", csInfo.version.isEmpty() ? "Unknown" : csInfo.version.c_str());
  _csBoard = lv_label_create(content);
  lv_label_set_text_fmt(_csBoard, "Board: %s", csInfo.board.isEmpty() ? "Unknown" : csInfo.board.c_str());
  _csShield = lv_label_create(content);
  lv_label_set_text_fmt(_csShield, "Shield: %s", csInfo.shield.isEmpty() ? "Unknown" : csInfo.shield.c_str());
  _csBuild = lv_label_create(content);
  lv_label_set_text_fmt(_csBuild, "Build: %s", csInfo.build.isEmpty() ? "Unknown" : csInfo.build.c_str());

  _updateTimer = lv_timer_create(update_timer_cb, 2000, this);
}

AboutUI::~AboutUI() {
  if (_updateTimer) lv_timer_del(_updateTimer);
  if (_container)   lv_obj_del(_container);
}

void AboutUI::update_timer_cb(lv_timer_t* timer) {
  AboutUI* ui = (AboutUI*)lv_timer_get_user_data(timer);
  if (!ui) return;

  if (ui->_memLbl)
      lv_label_set_text_fmt(ui->_memLbl, "Free RAM: %d KB", ESP.getFreeHeap() / 1024);

  if (ui->_wifiStat) {
      if (WiFi.status() == WL_CONNECTED)
          lv_label_set_text_fmt(ui->_wifiStat, "WiFi: Connected\nIP: %s\nmDNS: dcc-ex-cyd.local", WiFi.localIP().toString().c_str());
      else
          lv_label_set_text(ui->_wifiStat, "WiFi: Disconnected");
  }

  if (ui->_sdStat) {
      uint8_t cardType = SD.cardType();
      if (cardType == CARD_NONE) {
          lv_label_set_text(ui->_sdStat, "Status: Not Attached");
      } else {
          const char* typeStr = "UNKNOWN";
          if (cardType == CARD_MMC)  typeStr = "MMC";
          else if (cardType == CARD_SD)   typeStr = "SDSC";
          else if (cardType == CARD_SDHC) typeStr = "SDHC";
          uint64_t cardSize = SD.cardSize() / (1024 * 1024);
          lv_label_set_text_fmt(ui->_sdStat, "Status: Mounted\nFormat: %s\nCapacity: %llu MB", typeStr, cardSize);
      }
  }

  // Update CS info from global struct (populated by AppDelegate on connect)
  if (ui->_csConnected)
      lv_label_set_text(ui->_csConnected, csInfo.connected ? "Status: Connected" : "Status: Not Connected");
  if (ui->_csVersion)
      lv_label_set_text_fmt(ui->_csVersion, "Version: %s", csInfo.version.isEmpty() ? "Unknown" : csInfo.version.c_str());
  if (ui->_csBoard)
      lv_label_set_text_fmt(ui->_csBoard, "Board: %s", csInfo.board.isEmpty() ? "Unknown" : csInfo.board.c_str());
  if (ui->_csShield)
      lv_label_set_text_fmt(ui->_csShield, "Shield: %s", csInfo.shield.isEmpty() ? "Unknown" : csInfo.shield.c_str());
  if (ui->_csBuild)
      lv_label_set_text_fmt(ui->_csBuild, "Build: %s", csInfo.build.isEmpty() ? "Unknown" : csInfo.build.c_str());
}

void AboutUI::close_btn_event_cb(lv_event_t * e) {
  AboutUI* ui = (AboutUI*)lv_event_get_user_data(e);
  lv_obj_add_flag(ui->_container, LV_OBJ_FLAG_HIDDEN);
}
