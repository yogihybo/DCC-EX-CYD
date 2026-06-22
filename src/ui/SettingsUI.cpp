#include "SettingsUI.h"
#include "Theme.h"
#include "WiFiUI.h"
#include "AboutUI.h"
#include "ProgramUI.h"
#include "../utils/Screenshot.h"
#include <SD.h>

// Static flag – readable by ThrottleServer to gate web access
bool SettingsUI::throttleProgrammingActive = false;

SettingsUI::SettingsUI(DCCEXProtocol& dccex, lv_obj_t* parent) : _dccex(dccex), _wifiUI(nullptr), _aboutUI(nullptr), _calibrationUI(nullptr), _programUI(nullptr) {
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_pad_all(_container, 5, 0);
  lv_obj_set_style_pad_hor(_container, 11, 0);
  lv_obj_set_style_pad_row(_container, 3, 0);
  lv_obj_set_style_border_width(_container, 0, 0);
  lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  auto add_section = [this](const char* title) {
    lv_obj_t* lbl = lv_label_create(_container);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, tc(TC_SECTION), 0);
    lv_obj_set_width(lbl, LV_PCT(100));
    lv_obj_set_style_pad_top(lbl, 6, 0);
  };

  auto make_row = [this](lv_event_cb_t cb) -> lv_obj_t* {
    lv_obj_t* btn = lv_btn_create(_container);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 32);
    lv_obj_set_style_pad_ver(btn, 0, 0);
    lv_obj_set_style_pad_hor(btn, 7, 0);
    lv_obj_set_style_bg_color(btn, tc(TC_SURFACE_RAISED), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, this);
    return btn;
  };

  auto make_name = [](lv_obj_t* btn, const char* name) {
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, name);
    lv_obj_set_flex_grow(lbl, 1);
  };

  auto make_badge = [](lv_obj_t* btn, const char* val, lv_color_t bg, lv_color_t fg) -> lv_obj_t* {
    lv_obj_t* badge = lv_label_create(btn);
    lv_label_set_text(badge, val);
    lv_obj_set_style_bg_color(badge, bg, 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(badge, fg, 0);
    lv_obj_set_style_pad_hor(badge, 5, 0);
    lv_obj_set_style_pad_ver(badge, 2, 0);
    lv_obj_set_style_radius(badge, 3, 0);
    return badge;
  };

  auto make_chevron = [](lv_obj_t* btn) {
    lv_obj_t* ch = lv_label_create(btn);
    lv_label_set_text(ch, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_color(ch, tc(TC_TEXT_MUTED), 0);
  };

  const lv_color_t VAL_BG = tc(TC_BADGE_BG);
  const lv_color_t VAL_FG = tc(TC_BADGE_FG);
  static const lv_color_t ERR_BG = lv_color_hex(0x3a1414);
  static const lv_color_t ERR_FG = lv_color_make(200, 60, 60);
  static const char* rotLabels[] = { "USB Down", "USB Up" };
  static const char* accelLabels[] = { "Off", "Slow", "Med", "Fast" };

  lv_obj_t* b;
  char buf[16];

  // -------------------------------------------------------
  // PROGRAMMING
  // -------------------------------------------------------
  add_section("Programming");

  b = make_row(programming_setup_event_cb);
  make_name(b, "Loco programming");
  make_chevron(b);

  b = make_row(throttle_programming_event_cb);
  make_name(b, "Throttle programming");
  make_chevron(b);

  // -------------------------------------------------------
  // THROTTLE
  // -------------------------------------------------------
  add_section("Throttle");

  b = make_row(brightness_btn_event_cb);
  make_name(b, "Brightness");
  snprintf(buf, sizeof(buf), "%d%%", (Settings.brightness * 100) / 255);
  _brightnessLbl = make_badge(b, buf, VAL_BG, VAL_FG);

  b = make_row(theme_event_cb);
  make_name(b, "Theme");
  _themeLbl = make_badge(b, Settings.theme == SettingsClass::Theme::DARK ? "Dark" : "Light", VAL_BG, VAL_FG);

  b = make_row(rotation_event_cb);
  make_name(b, "Rotation");
  _rotationLbl = make_badge(b, rotLabels[Settings.rotation], VAL_BG, VAL_FG);

  b = make_row(calibrate_event_cb);
  make_name(b, "Calibrate touch screen");
  make_chevron(b);

  b = make_row(speed_step_event_cb);
  make_name(b, "Encoder sensitivity");
  snprintf(buf, sizeof(buf), "%d", 1 << Settings.LocoUI.speedStep);
  _speedStepLbl = make_badge(b, buf, VAL_BG, VAL_FG);

  b = make_row(accel_event_cb);
  make_name(b, "Encoder acceleration");
  _accelLbl = make_badge(b, accelLabels[Settings.LocoUI.acceleration], VAL_BG, VAL_FG);

  b = make_row(estop_delay_event_cb);
  make_name(b, "E-stop hold time");
  snprintf(buf, sizeof(buf), "%ds", Settings.emergencyStopDelay);
  _eStopDelayLbl = make_badge(b, buf, VAL_BG, VAL_FG);

  // -------------------------------------------------------
  // STORAGE
  // -------------------------------------------------------
  add_section("Storage");

  b = make_row(storage_mode_event_cb);
  make_name(b, "Storage location");
  _storageModeLbl = make_badge(b, Settings.storageMode == SettingsClass::StorageMode::SD_CARD ? "SD Card" : "Internal", VAL_BG, VAL_FG);

  b = make_row(sd_format_event_cb);
  make_name(b, "Format SD card");
  make_badge(b, "Erase", ERR_BG, ERR_FG);

  // -------------------------------------------------------
  // CONNECTIONS
  // -------------------------------------------------------
  add_section("Connections");

  b = make_row(wifi_setup_event_cb);
  make_name(b, "WiFi setup");
  make_chevron(b);

  b = make_row(ap_mode_event_cb);
  make_name(b, "Access point");
  _apModeLbl = make_badge(b,
    Settings.AP.enabled ? "ON" : "OFF",
    Settings.AP.enabled ? lv_color_hex(0x1b3a1b) : tc(TC_SURFACE_DEEP),
    Settings.AP.enabled ? lv_color_make(76, 175, 80) : tc(TC_TEXT_MUTED));

  // -------------------------------------------------------
  // ABOUT
  // -------------------------------------------------------
  add_section("About");

  b = make_row(about_event_cb);
  make_name(b, "About DCC-EX-CYD");
  make_chevron(b);

  b = make_row(screenshot_event_cb);
  make_name(b, "Screenshot (3s delay)");
  make_chevron(b);

  // Demo mode button — uncomment block below + include DemoMode.h to re-enable
  // b = make_row(DemoMode::trigger); make_name(b, "Demo mode");
  // make_badge(b, "TEMP", lv_color_hex(0x3a2e14), lv_color_make(200, 150, 50)); make_chevron(b);
}

SettingsUI::~SettingsUI() {
  Settings.save();
  if (_wifiUI) delete _wifiUI;
  if (_aboutUI) delete _aboutUI;
  if (_calibrationUI) delete _calibrationUI;
  if (_programUI) delete _programUI;
  if (_formatMsgbox) { lv_msgbox_close(_formatMsgbox); _formatMsgbox = nullptr; }
  if (_calMsgbox)    { lv_msgbox_close(_calMsgbox);    _calMsgbox    = nullptr; }
  if (_container) lv_obj_del(_container);
}

void SettingsUI::speed_step_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (++Settings.LocoUI.speedStep > 2) Settings.LocoUI.speedStep = 0;
  lv_label_set_text_fmt(ui->_speedStepLbl, "%d", 1 << Settings.LocoUI.speedStep);
  Settings.save();
}

void SettingsUI::accel_event_cb(lv_event_t * e) {
  static const char* accelLabels[] = { "Off", "Slow", "Med", "Fast" };
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (++Settings.LocoUI.acceleration > 3) Settings.LocoUI.acceleration = 0;
  lv_label_set_text_fmt(ui->_accelLbl, "%s", accelLabels[Settings.LocoUI.acceleration]);
  Settings.save();
}

void SettingsUI::estop_delay_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (++Settings.emergencyStopDelay > 10) Settings.emergencyStopDelay = 1;
  lv_label_set_text_fmt(ui->_eStopDelayLbl, "%ds", Settings.emergencyStopDelay);
  Settings.save();
}

void SettingsUI::theme_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  Settings.theme = (Settings.theme == SettingsClass::Theme::DARK) ? SettingsClass::Theme::LIGHT : SettingsClass::Theme::DARK;
  lv_label_set_text_fmt(ui->_themeLbl, "%s", Settings.theme == SettingsClass::Theme::DARK ? "Dark" : "Light");
  Settings.save();
  Settings.dispatchEvent(SettingsClass::Event::THEME_CHANGE);
}

void SettingsUI::rotation_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (++Settings.rotation > 1) Settings.rotation = 0;
  static const char* rotLabels[] = { "USB Down", "USB Up" };
  lv_label_set_text(ui->_rotationLbl, rotLabels[Settings.rotation]);
  Settings.save();
  Settings.dispatchEvent(SettingsClass::Event::ROTATION_CHANGE);
}


void SettingsUI::storage_mode_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  Settings.storageMode = Settings.storageMode == SettingsClass::StorageMode::SD_CARD ? SettingsClass::StorageMode::LITTLEFS : SettingsClass::StorageMode::SD_CARD;
  lv_label_set_text_fmt(ui->_storageModeLbl, "%s", Settings.storageMode == SettingsClass::StorageMode::SD_CARD ? "SD Card" : "Internal");
  Settings.save();
}

void SettingsUI::ap_mode_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  Settings.AP.enabled = !Settings.AP.enabled;
  bool apOn = Settings.AP.enabled;
  lv_label_set_text(ui->_apModeLbl, apOn ? "ON" : "OFF");
  lv_obj_set_style_bg_color(ui->_apModeLbl, apOn ? lv_color_hex(0x1b3a1b) : tc(TC_SURFACE_DEEP), 0);
  lv_obj_set_style_text_color(ui->_apModeLbl, apOn ? lv_color_make(76, 175, 80) : tc(TC_TEXT_MUTED), 0);
  Settings.save();
  // Dispatch CS_CHANGE to trigger network stack updates in main.cpp
  Settings.dispatchEvent(SettingsClass::Event::CS_CHANGE);
}

static void style_msgbox(lv_obj_t* mbox, const char* title_text, lv_color_t title_color) {
    lv_obj_set_width(mbox, LV_PCT(88));
    lv_obj_set_style_bg_color(mbox, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_color(mbox, tc(TC_OVERLAY_BORDER), 0);
    lv_obj_set_style_border_width(mbox, 1, 0);
    lv_obj_t* title = lv_msgbox_add_title(mbox, title_text);
    lv_obj_set_style_text_color(title, title_color, 0);
}

static void style_msgbox_text(lv_obj_t* txt) {
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(txt, tc(TC_OVERLAY_TEXT), 0);
}

void SettingsUI::sd_format_event_cb(lv_event_t * e) {
    SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
    if (ui->_formatMsgbox) return;

    ui->_formatMsgbox = lv_msgbox_create(lv_layer_top());
    style_msgbox(ui->_formatMsgbox, "Format SD card?", lv_color_make(200, 60, 60));
    style_msgbox_text(lv_msgbox_add_text(ui->_formatMsgbox, "Formatting will erase all data on the SD card.\nThe throttle will restart if successful."));

    lv_obj_t* format_btn = lv_msgbox_add_footer_button(ui->_formatMsgbox, "Format");
    lv_obj_set_style_bg_color(format_btn, tc(TC_DANGER), 0);
    lv_obj_set_style_text_color(format_btn, lv_color_hex(0xffffff), 0);
    lv_obj_add_event_cb(format_btn, sd_format_confirm_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t* cancel_btn = lv_msgbox_add_footer_button(ui->_formatMsgbox, "Cancel");
    lv_obj_set_style_bg_color(cancel_btn, tc(TC_SURFACE_RAISED), 0);
    lv_obj_add_event_cb(cancel_btn, [](lv_event_t* e) {
        SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
        if (ui->_formatMsgbox) {
            lv_msgbox_close(ui->_formatMsgbox);
            ui->_formatMsgbox = nullptr;
        }
    }, LV_EVENT_CLICKED, ui);

    lv_obj_center(ui->_formatMsgbox);
}

void SettingsUI::sd_format_confirm_event_cb(lv_event_t * e) {
    SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
    if (ui->_formatMsgbox) {
        lv_msgbox_close(ui->_formatMsgbox);
        ui->_formatMsgbox = nullptr;
    }

    if (SD.begin(5, SPI, 4000000, "/sd", 5, true)) {
        ESP.restart();
    } else {
        ui->_formatMsgbox = lv_msgbox_create(lv_layer_top());
        style_msgbox(ui->_formatMsgbox, "Format failed", lv_color_make(200, 60, 60));
        style_msgbox_text(lv_msgbox_add_text(ui->_formatMsgbox, "Unable to mount SD card. Check it is inserted correctly."));

        lv_obj_t* ok_btn = lv_msgbox_add_footer_button(ui->_formatMsgbox, "OK");
        lv_obj_set_style_bg_color(ok_btn, tc(TC_SURFACE_RAISED), 0);
        lv_obj_add_event_cb(ok_btn, [](lv_event_t* e) {
            SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
            if (ui->_formatMsgbox) {
                lv_msgbox_close(ui->_formatMsgbox);
                ui->_formatMsgbox = nullptr;
            }
        }, LV_EVENT_CLICKED, ui);

        lv_obj_center(ui->_formatMsgbox);
    }
}


void SettingsUI::brightness_btn_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);

  lv_obj_t* mbox = lv_msgbox_create(lv_layer_top());
  style_msgbox(mbox, "Brightness", tc(TC_SECTION));

  lv_obj_t* slider = lv_slider_create(mbox);
  lv_obj_set_width(slider, LV_PCT(100));
  lv_obj_set_style_margin_top(slider, 12, 0);
  lv_obj_set_style_margin_bottom(slider, 12, 0);
  lv_obj_set_style_margin_hor(slider, 12, 0);
  lv_obj_set_style_height(slider, 4, LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, tc(TC_BORDER), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, tc(TC_SECTION), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0xdddddd), LV_PART_KNOB);
  lv_obj_set_style_pad_all(slider, 6, LV_PART_KNOB);
  lv_slider_set_range(slider, 10, 255);
  lv_slider_set_value(slider, Settings.brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, brightness_event_cb, LV_EVENT_VALUE_CHANGED, ui);

  lv_obj_t* close_btn = lv_msgbox_add_footer_button(mbox, "Close");
  lv_obj_set_style_bg_color(close_btn, tc(TC_SURFACE_RAISED), 0);
  lv_obj_add_event_cb(close_btn, [](lv_event_t* e) {
      lv_msgbox_close((lv_obj_t*)lv_event_get_user_data(e));
  }, LV_EVENT_CLICKED, mbox);

  lv_obj_center(mbox);
}

void SettingsUI::brightness_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
  Settings.brightness = lv_slider_get_value(slider);
  lv_label_set_text_fmt(ui->_brightnessLbl, "%d%%", (Settings.brightness * 100) / 255);
  Settings.save();
  Settings.dispatchEvent(SettingsClass::Event::BRIGHTNESS_CHANGE);
}

void SettingsUI::wifi_setup_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (!ui->_wifiUI) {
      ui->_wifiUI = new WiFiUI(lv_layer_top());
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
  ui->_aboutUI = new AboutUI(ui->_dccex, lv_layer_top());
}

void SettingsUI::programming_setup_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (ui->_programUI) {
      delete ui->_programUI;
  }
  ui->_programUI = new ProgramUI(ui->_dccex, lv_layer_top());
}

void SettingsUI::calibrate_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (ui->_calMsgbox) return;

  ui->_calMsgbox = lv_msgbox_create(lv_layer_top());
  style_msgbox(ui->_calMsgbox, "Touch calibration", tc(TC_SECTION));
  style_msgbox_text(lv_msgbox_add_text(ui->_calMsgbox, "Use a stylus to precisely tap the targets.\nIncorrect calibration will make the screen unusable."));

  lv_obj_t* start_btn = lv_msgbox_add_footer_button(ui->_calMsgbox, "Start");
  lv_obj_set_style_bg_color(start_btn, lv_color_make(40, 140, 40), 0);
  lv_obj_add_event_cb(start_btn, [](lv_event_t* e) {
      SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
      if (ui->_calMsgbox) {
          lv_msgbox_close(ui->_calMsgbox);
          ui->_calMsgbox = nullptr;
      }
      if (!ui->_calibrationUI) {
          ui->_calibrationUI = new CalibrationUI();
      }
      ui->_calibrationUI->show();
  }, LV_EVENT_CLICKED, ui);

  lv_obj_t* cancel_btn = lv_msgbox_add_footer_button(ui->_calMsgbox, "Cancel");
  lv_obj_set_style_bg_color(cancel_btn, tc(TC_SURFACE_RAISED), 0);

  lv_obj_add_event_cb(cancel_btn, [](lv_event_t* e) {
      SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
      if (ui->_calMsgbox) {
          lv_msgbox_close(ui->_calMsgbox);
          ui->_calMsgbox = nullptr;
      }
  }, LV_EVENT_CLICKED, ui);

  lv_obj_center(ui->_calMsgbox);
}

static void screenshot_timer_cb(lv_timer_t* timer) {
  lv_timer_del(timer);
  
  static int counter = 1;
  char filename[32];
  snprintf(filename, sizeof(filename), "/screenshot_%d.bmp", counter);
  
  // Find the next available sequential filename
  while(SD.exists(filename)) {
      counter++;
      snprintf(filename, sizeof(filename), "/screenshot_%d.bmp", counter);
  }

  bool success = saveScreenshot(filename);
  
  lv_obj_t* mbox = lv_msgbox_create(lv_layer_top());
  lv_msgbox_add_title(mbox, success ? "Screenshot Saved" : "Screenshot Failed");
  
  char msg[64];
  if (success) {
      snprintf(msg, sizeof(msg), "Screenshot saved to %s", filename);
  } else {
      snprintf(msg, sizeof(msg), "Failed to save screenshot. Check SD Card.");
  }
  lv_msgbox_add_text(mbox, msg);
  
  lv_obj_t* ok_btn = lv_msgbox_add_footer_button(mbox, "OK");
  lv_obj_add_event_cb(ok_btn, [](lv_event_t* e) {
      lv_obj_t* box = (lv_obj_t*)lv_event_get_user_data(e);
      lv_msgbox_close(box);
  }, LV_EVENT_CLICKED, mbox);
  
  lv_obj_center(mbox);
}

void SettingsUI::screenshot_event_cb(lv_event_t * e) {
  lv_timer_create(screenshot_timer_cb, 3000, nullptr);
}

// ---------------------------------------------------------------------------
// Demo Mode — logic moved to src/utils/DemoMode.h / DemoMode.cpp
// To re-enable: uncomment the include below, the button row above,
//               and demoStep() in LocoUI.h/.cpp and PowerUI.h/.cpp
// ---------------------------------------------------------------------------
// #include "../utils/DemoMode.h"

// ---------------------------------------------------------------------------
// Throttle Programming Mode
// ---------------------------------------------------------------------------
void SettingsUI::throttle_programming_event_cb(lv_event_t * e) {
  // Mark programming mode active – ThrottleServer reads this flag to serve the full web UI
  throttleProgrammingActive = true;

  // Signal main.cpp to destroy all major UI objects to free heap for the web server
  Settings.dispatchEvent(SettingsClass::Event::THROTTLE_PROGRAM_ENTER);

  // NOTE: at this point our own SettingsUI (and its parent tabview) has been deleted
  // by main.cpp's event handler. We must not access 'this' after this point.
  // The overlay is created on lv_scr_act() so it survives the tabview deletion.

  lv_obj_t* scr = lv_scr_act();
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Full-screen overlay container
  lv_obj_t* overlay = lv_obj_create(scr);
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_align(overlay, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_set_style_radius(overlay, 0, 0);
  lv_obj_set_flex_flow(overlay, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(overlay, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);

  // Status label
  lv_obj_t* lbl = lv_label_create(overlay);
  lv_label_set_text(lbl, LV_SYMBOL_SETTINGS "  Throttle Programming Mode");
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_bottom(lbl, 8, 0);

  // Connection info
  bool staConnected = WiFi.status() == WL_CONNECTED;
  String ip = staConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
  lv_obj_t* ip_lbl = lv_label_create(overlay);
  if (staConnected)
    lv_label_set_text_fmt(ip_lbl, "Connect to throttle via web browser:\nhttp://%s\nhttp://dcc-ex-cyd.local", ip.c_str());
  else
    lv_label_set_text_fmt(ip_lbl, "Connect to throttle via web browser:\nhttp://%s", ip.c_str());
  lv_obj_set_style_text_font(ip_lbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(ip_lbl, lv_color_hex(0x4488ff), 0);
  lv_obj_set_style_text_align(ip_lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_bottom(ip_lbl, 20, 0);

  // Red close button
  lv_obj_t* close_btn = make_danger_btn(overlay, LV_SYMBOL_CLOSE "  Close");
  lv_obj_set_width(close_btn, LV_PCT(60));

  // Pass overlay pointer via user data so the callback can delete it
  lv_obj_add_event_cb(close_btn, [](lv_event_t* e) {
    lv_obj_t* overlay = (lv_obj_t*)lv_event_get_user_data(e);

    // Clear the programming mode flag before rebuilding the UI
    SettingsUI::throttleProgrammingActive = false;

    // Delete the overlay first so the rebuilt tabview renders cleanly
    lv_obj_del(overlay);

    // Signal main.cpp to recreate the full tabview and all UI objects
    Settings.dispatchEvent(SettingsClass::Event::THROTTLE_PROGRAM_EXIT);

  }, LV_EVENT_CLICKED, overlay);
}
