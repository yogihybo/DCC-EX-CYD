#include "AboutUI.h"
#include <Version.h>
#include <WiFi.h>
#include <Settings.h>
#include <SD.h>

AboutUI::AboutUI(DCCExCS& dccExCS, lv_obj_t* parent) : _dccExCS(dccExCS) {
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
  lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(_container, 0, 0);
  lv_obj_set_style_border_width(_container, 0, 0);
  lv_obj_set_style_bg_opa(_container, LV_OPA_COVER, 0);

  lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(_container, 2, 0);
  lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);

  // Top header with Back button
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

#if LV_USE_FONT_MONTSERRAT_12
  lv_obj_set_style_text_font(_container, &lv_font_montserrat_12, 0);
#endif

  // Content
  String version("T3-EX-WiFi Version: ");
  version += T3_VERSION_MAJOR; version += "."; version += T3_VERSION_MINOR; version += "."; version += T3_VERSION_PATCH;
  lv_obj_t* t3_ver = lv_label_create(_container);
  lv_label_set_text(t3_ver, version.c_str());

  lv_obj_t* platform_lbl = lv_label_create(_container);
  lv_label_set_text_fmt(platform_lbl, "Platform: ESP32 (%s)", ESP.getChipModel());

  _memLbl = lv_label_create(_container);
  lv_label_set_text_fmt(_memLbl, "Free RAM: %d KB", ESP.getFreeHeap() / 1024);

  // WiFi Status
  _wifiStat = lv_label_create(_container);
  if (WiFi.status() == WL_CONNECTED) {
      lv_label_set_text_fmt(_wifiStat, "WiFi: Connected\nIP: %s", WiFi.localIP().toString().c_str());
  } else {
      lv_label_set_text(_wifiStat, "WiFi: Disconnected");
  }

  lv_obj_t* apStat = lv_label_create(_container);
  lv_label_set_text_fmt(apStat, "AP Name: %s\nAP Password: %s", Settings.AP.SSID.c_str(), Settings.AP.password.c_str());

#if 0 // SD Card unsupported on CYD hardware due to SPI conflict
  // SD Card Info
  lv_obj_t* sd_title = lv_label_create(_container);
  lv_label_set_text(sd_title, "SD Card Info");
  lv_obj_set_style_text_color(sd_title, lv_color_make(52, 204, 211), 0); // Cyan color
  lv_obj_set_style_pad_top(sd_title, 10, 0);

  lv_obj_t* sdStat = lv_label_create(_container);
  
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
      lv_label_set_text(sdStat, "Status: Not Attached");
  } else {
      const char* typeStr = "Unknown";
      if (cardType == CARD_MMC) typeStr = "MMC";
      else if (cardType == CARD_SD) typeStr = "SDSC";
      else if (cardType == CARD_SDHC) typeStr = "SDHC";
      
      uint64_t cardSize = SD.cardSize() / (1024 * 1024);
      lv_label_set_text_fmt(sdStat, "Status: Mounted\nFormat: %s\nCapacity: %llu MB", typeStr, cardSize);
  }
#endif

  _updateTimer = lv_timer_create(update_timer_cb, 2000, this);

  // DCC EX Section
  lv_obj_t* cs_title = lv_label_create(_container);
  lv_label_set_text(cs_title, "About DCC-EX Command Station");
  lv_obj_set_style_text_color(cs_title, lv_color_make(52, 204, 211), 0); // Cyan color
  lv_obj_set_style_pad_top(cs_title, 10, 0);

  _csVersion = lv_label_create(_container);
  lv_label_set_text(_csVersion, "Version: Loading...");
  _csBoard = lv_label_create(_container);
  lv_label_set_text(_csBoard, "Board: Loading...");
  _csShield = lv_label_create(_container);
  lv_label_set_text(_csShield, "Shield: Loading...");
  _csBuild = lv_label_create(_container);
  lv_label_set_text(_csBuild, "Build: Loading...");

  _csVersionHandler = _dccExCS.addEventListener(DCCExCS::Event::VERSION, [this](void* parameter) {
    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        auto version = *static_cast<DCCExCS::Version*>(parameter);
        lv_label_set_text_fmt(_csVersion, "Version: %s", version.version.c_str());
        lv_label_set_text_fmt(_csBoard, "Board: %s", version.board.c_str());
        lv_label_set_text_fmt(_csShield, "Shield: %s", version.shield.c_str());
        lv_label_set_text_fmt(_csBuild, "Build: %s", version.build.c_str());
        xSemaphoreGive(lvgl_mutex);
    }
  });

  _dccExCS.getCSVersion();
}

AboutUI::~AboutUI() {
  if (_updateTimer) lv_timer_del(_updateTimer);
  _dccExCS.removeEventListener(DCCExCS::Event::VERSION, _csVersionHandler);
  if (_container) lv_obj_del(_container);
}

void AboutUI::update_timer_cb(lv_timer_t* timer) {
  AboutUI* ui = (AboutUI*)lv_timer_get_user_data(timer);
  if (ui && ui->_memLbl && ui->_wifiStat) {
      lv_label_set_text_fmt(ui->_memLbl, "Free RAM: %d KB", ESP.getFreeHeap() / 1024);
      if (WiFi.status() == WL_CONNECTED) {
          lv_label_set_text_fmt(ui->_wifiStat, "WiFi: Connected\nIP: %s", WiFi.localIP().toString().c_str());
      } else {
          lv_label_set_text(ui->_wifiStat, "WiFi: Disconnected");
      }
  }
}

void AboutUI::close_btn_event_cb(lv_event_t * e) {
  AboutUI* ui = (AboutUI*)lv_event_get_user_data(e);
  lv_obj_add_flag(ui->_container, LV_OBJ_FLAG_HIDDEN);
}
