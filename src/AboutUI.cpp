#include "AboutUI.h"
#include <Version.h>
#include <WiFi.h>

AboutUI::AboutUI(DCCExCS& dccExCS, lv_obj_t* parent) : _dccExCS(dccExCS) {
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(_container, 0, 0);
  lv_obj_set_style_border_width(_container, 0, 0);
  lv_obj_set_style_bg_opa(_container, 0, 0);
  lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(_container, 2, 0);
  lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);

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


