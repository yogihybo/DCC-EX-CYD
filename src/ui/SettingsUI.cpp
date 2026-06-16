#include "SettingsUI.h"
#include "WiFiUI.h"
#include "AboutUI.h"
#include "ProgramUI.h"
#include "../utils/Screenshot.h"
#include <SD.h>

// Static flag – readable by ThrottleServer to gate web access
bool SettingsUI::throttleProgrammingActive = false;

SettingsUI::SettingsUI(DCCExCS& dccExCS, lv_obj_t* parent) : _dccExCS(dccExCS), _wifiUI(nullptr), _aboutUI(nullptr), _calibrationUI(nullptr), _programUI(nullptr) {
  _container = lv_obj_create(parent);
  lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
  lv_obj_align(_container, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_pad_all(_container, 10, 0);
  lv_obj_set_style_border_width(_container, 0, 0);
  
  // Set up the container as a vertically scrolling flex list
  lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Helper lambda for category headers
  auto add_category = [this](const char* title) {
    lv_obj_t* lbl = lv_label_create(_container);
    lv_label_set_text_fmt(lbl, "--- %s ---", title);
    lv_obj_set_style_text_color(lbl, lv_color_make(150, 150, 150), 0);
    lv_obj_set_style_pad_top(lbl, 10, 0);
    lv_obj_set_style_pad_bottom(lbl, 5, 0);
  };

  // -------------------------------------------------------
  // PROGRAMMING
  // -------------------------------------------------------
  add_category("Programming");

  lv_obj_t* program_btn = lv_btn_create(_container);
  lv_obj_set_width(program_btn, LV_PCT(100));
  lv_obj_t* program_lbl = lv_label_create(program_btn);
  lv_label_set_text(program_lbl, "Loco Programming");
  lv_obj_center(program_lbl);
  lv_obj_add_event_cb(program_btn, programming_setup_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* estop_btn = lv_btn_create(_container);
  lv_obj_set_width(estop_btn, LV_PCT(100));
  _eStopDelayLbl = lv_label_create(estop_btn);
  lv_label_set_text_fmt(_eStopDelayLbl, "E-Stop Hold Time: %ds", Settings.emergencyStopDelay);
  lv_obj_center(_eStopDelayLbl);
  lv_obj_add_event_cb(estop_btn, estop_delay_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* tprog_btn = lv_btn_create(_container);
  lv_obj_set_width(tprog_btn, LV_PCT(100));
  lv_obj_t* tprog_lbl = lv_label_create(tprog_btn);
  lv_label_set_text(tprog_lbl, "Throttle Programming");
  lv_obj_center(tprog_lbl);
  lv_obj_add_event_cb(tprog_btn, throttle_programming_event_cb, LV_EVENT_CLICKED, this);

  // -------------------------------------------------------
  // THROTTLE
  // -------------------------------------------------------
  add_category("Throttle");

  lv_obj_t* br_btn = lv_btn_create(_container);
  lv_obj_set_width(br_btn, LV_PCT(100));
  _brightnessLbl = lv_label_create(br_btn);
  lv_label_set_text_fmt(_brightnessLbl, "Brightness: %d%%", (Settings.brightness * 100) / 255);
  lv_obj_center(_brightnessLbl);
  lv_obj_add_event_cb(br_btn, brightness_btn_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* theme_btn = lv_btn_create(_container);
  lv_obj_set_width(theme_btn, LV_PCT(100));
  _themeLbl = lv_label_create(theme_btn);
  lv_label_set_text_fmt(_themeLbl, "Theme: %s", Settings.theme == SettingsClass::Theme::DARK ? "Dark" : "Light");
  lv_obj_center(_themeLbl);
  lv_obj_add_event_cb(theme_btn, theme_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* rotation_btn = lv_btn_create(_container);
  lv_obj_set_width(rotation_btn, LV_PCT(100));
  _rotationLbl = lv_label_create(rotation_btn);
  lv_label_set_text_fmt(_rotationLbl, "Rotation: %d", Settings.rotation);
  lv_obj_center(_rotationLbl);
  lv_obj_add_event_cb(rotation_btn, rotation_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* cal_btn = lv_btn_create(_container);
  lv_obj_set_width(cal_btn, LV_PCT(100));
  lv_obj_t* cal_lbl = lv_label_create(cal_btn);
  lv_label_set_text(cal_lbl, "Calibrate Touch Screen");
  lv_obj_center(cal_lbl);
  lv_obj_add_event_cb(cal_btn, calibrate_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* speed_btn = lv_btn_create(_container);
  lv_obj_set_width(speed_btn, LV_PCT(100));
  _speedStepLbl = lv_label_create(speed_btn);
  lv_label_set_text_fmt(_speedStepLbl, "Throttle Speed Step: %d", Settings.LocoUI.speedStep);
  lv_obj_center(_speedStepLbl);
  lv_obj_add_event_cb(speed_btn, speed_step_event_cb, LV_EVENT_CLICKED, this);

  // -------------------------------------------------------
  // STORAGE
  // -------------------------------------------------------
  add_category("Storage");

  lv_obj_t* storage_btn = lv_btn_create(_container);
  lv_obj_set_width(storage_btn, LV_PCT(100));
  _storageModeLbl = lv_label_create(storage_btn);
  lv_label_set_text_fmt(_storageModeLbl, "Storage Location: %s", Settings.storageMode == SettingsClass::StorageMode::SD_CARD ? "SD Card" : "Internal");
  lv_obj_center(_storageModeLbl);
  lv_obj_add_event_cb(storage_btn, storage_mode_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* sd_format_btn = lv_btn_create(_container);
  lv_obj_set_width(sd_format_btn, LV_PCT(100));
  lv_obj_t* sd_format_lbl = lv_label_create(sd_format_btn);
  lv_label_set_text(sd_format_lbl, "Format SD Card");
  lv_obj_center(sd_format_lbl);
  lv_obj_add_event_cb(sd_format_btn, sd_format_event_cb, LV_EVENT_CLICKED, this);

  // -------------------------------------------------------
  // CONNECTIONS
  // -------------------------------------------------------
  add_category("Connections");

  lv_obj_t* wifi_btn = lv_btn_create(_container);
  lv_obj_set_width(wifi_btn, LV_PCT(100));
  lv_obj_t* wifi_lbl = lv_label_create(wifi_btn);
  lv_label_set_text(wifi_lbl, "WiFi Setup");
  lv_obj_center(wifi_lbl);
  lv_obj_add_event_cb(wifi_btn, wifi_setup_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* ap_btn = lv_btn_create(_container);
  lv_obj_set_width(ap_btn, LV_PCT(100));
  _apModeLbl = lv_label_create(ap_btn);
  lv_label_set_text_fmt(_apModeLbl, "Access Point (SoftAP): %s", Settings.AP.enabled ? "ON" : "OFF");
  lv_obj_center(_apModeLbl);
  lv_obj_add_event_cb(ap_btn, ap_mode_event_cb, LV_EVENT_CLICKED, this);

  // -------------------------------------------------------
  // ABOUT
  // -------------------------------------------------------
  add_category("About");

  lv_obj_t* about_btn = lv_btn_create(_container);
  lv_obj_set_width(about_btn, LV_PCT(100));
  lv_obj_t* about_lbl = lv_label_create(about_btn);
  lv_label_set_text(about_lbl, "About DCC-EX-CYD");
  lv_obj_center(about_lbl);
  lv_obj_add_event_cb(about_btn, about_event_cb, LV_EVENT_CLICKED, this);

  lv_obj_t* shot_btn = lv_btn_create(_container);
  lv_obj_set_width(shot_btn, LV_PCT(100));
  lv_obj_t* shot_lbl = lv_label_create(shot_btn);
  lv_label_set_text(shot_lbl, "Take Screenshot (3s Delay)");
  lv_obj_center(shot_lbl);
  lv_obj_add_event_cb(shot_btn, screenshot_event_cb, LV_EVENT_CLICKED, this);
}

SettingsUI::~SettingsUI() {
  Settings.save();
  if (_wifiUI) delete _wifiUI;
  if (_aboutUI) delete _aboutUI;
  if (_calibrationUI) delete _calibrationUI;
  if (_programUI) delete _programUI;
  if (_container) lv_obj_del(_container);
}

void SettingsUI::speed_step_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (++Settings.LocoUI.speedStep > 2) Settings.LocoUI.speedStep = 0;
  lv_label_set_text_fmt(ui->_speedStepLbl, "Throttle Speed Step: %d", Settings.LocoUI.speedStep);
  Settings.save();
}

void SettingsUI::estop_delay_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (++Settings.emergencyStopDelay > 10) Settings.emergencyStopDelay = 1;
  lv_label_set_text_fmt(ui->_eStopDelayLbl, "E-Stop Hold Time: %ds", Settings.emergencyStopDelay);
  Settings.save();
}

void SettingsUI::theme_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  Settings.theme = (Settings.theme == SettingsClass::Theme::DARK) ? SettingsClass::Theme::LIGHT : SettingsClass::Theme::DARK;
  lv_label_set_text_fmt(ui->_themeLbl, "Theme: %s", Settings.theme == SettingsClass::Theme::DARK ? "Dark" : "Light");
  Settings.save();
  Settings.dispatchEvent(SettingsClass::Event::THEME_CHANGE);
}

void SettingsUI::rotation_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (++Settings.rotation > 2) Settings.rotation = 0;
  lv_label_set_text_fmt(ui->_rotationLbl, "Rotation: %d", Settings.rotation);
  Settings.save();
  Settings.dispatchEvent(SettingsClass::Event::ROTATION_CHANGE);
}


void SettingsUI::storage_mode_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  Settings.storageMode = Settings.storageMode == SettingsClass::StorageMode::SD_CARD ? SettingsClass::StorageMode::LITTLEFS : SettingsClass::StorageMode::SD_CARD;
  lv_label_set_text_fmt(ui->_storageModeLbl, "Storage Location: %s", Settings.storageMode == SettingsClass::StorageMode::SD_CARD ? "SD Card" : "Internal");
  Settings.save();
}

void SettingsUI::ap_mode_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  Settings.AP.enabled = !Settings.AP.enabled;
  lv_label_set_text_fmt(ui->_apModeLbl, "Access Point (SoftAP): %s", Settings.AP.enabled ? "ON" : "OFF");
  Settings.save();
  // Dispatch CS_CHANGE to trigger network stack updates in main.cpp
  Settings.dispatchEvent(SettingsClass::Event::CS_CHANGE);
}

void SettingsUI::sd_format_event_cb(lv_event_t * e) {
    SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
    if (ui->_formatMsgbox) return; // Prevent multiple dialogs
    
    ui->_formatMsgbox = lv_msgbox_create(lv_layer_top());
    lv_msgbox_add_title(ui->_formatMsgbox, "Format SD Card?");
    lv_msgbox_add_text(ui->_formatMsgbox, "Formatting will erase the SD card.\nIf successful the throttle will restart.");
    
    lv_obj_t* format_btn = lv_msgbox_add_footer_button(ui->_formatMsgbox, "Format");
    lv_obj_add_event_cb(format_btn, sd_format_confirm_event_cb, LV_EVENT_CLICKED, ui);
    
    lv_obj_t* cancel_btn = lv_msgbox_add_footer_button(ui->_formatMsgbox, "Cancel");
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
        lv_msgbox_add_title(ui->_formatMsgbox, "Format Failed");
        lv_msgbox_add_text(ui->_formatMsgbox, "Unable to mount SD Card.");
        
        lv_obj_t* ok_btn = lv_msgbox_add_footer_button(ui->_formatMsgbox, "OK");
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
  lv_obj_set_width(mbox, LV_PCT(90));
  lv_msgbox_add_title(mbox, "Brightness");
  lv_msgbox_add_close_button(mbox);
  
  lv_obj_t* slider = lv_slider_create(mbox);
  lv_obj_set_width(slider, LV_PCT(100));
  lv_obj_set_style_margin_top(slider, 20, 0);
  lv_obj_set_style_margin_bottom(slider, 20, 0);
  lv_slider_set_range(slider, 10, 255);
  lv_slider_set_value(slider, Settings.brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(slider, brightness_event_cb, LV_EVENT_VALUE_CHANGED, ui);
  lv_obj_center(mbox);
}

void SettingsUI::brightness_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
  Settings.brightness = lv_slider_get_value(slider);
  lv_label_set_text_fmt(ui->_brightnessLbl, "Brightness: %d%%", (Settings.brightness * 100) / 255);
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
  ui->_aboutUI = new AboutUI(ui->_dccExCS, lv_layer_top());
}

void SettingsUI::programming_setup_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (ui->_programUI) {
      delete ui->_programUI;
  }
  ui->_programUI = new ProgramUI(ui->_dccExCS, lv_layer_top());
}

void SettingsUI::calibrate_event_cb(lv_event_t * e) {
  SettingsUI* ui = (SettingsUI*)lv_event_get_user_data(e);
  if (ui->_calMsgbox) return; // Prevent multiple dialogs
  
  ui->_calMsgbox = lv_msgbox_create(lv_layer_top());
  lv_msgbox_add_title(ui->_calMsgbox, "Touch Calibration");
  lv_msgbox_add_text(ui->_calMsgbox, "You are about to recalibrate the touchscreen.\n\nUse a stylus or pen to precisely tap the targets.\nIncorrect calibration will make the screen unusable!");
  
  lv_obj_t* start_btn = lv_msgbox_add_footer_button(ui->_calMsgbox, "Start");
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
  lv_obj_set_style_pad_bottom(lbl, 20, 0);

  // Red close button
  lv_obj_t* close_btn = lv_btn_create(overlay);
  lv_obj_set_style_bg_color(close_btn, lv_color_make(200, 30, 30), 0);
  lv_obj_set_style_bg_color(close_btn, lv_color_make(160, 20, 20), LV_STATE_PRESSED);
  lv_obj_set_width(close_btn, LV_PCT(60));

  lv_obj_t* close_lbl = lv_label_create(close_btn);
  lv_label_set_text(close_lbl, LV_SYMBOL_CLOSE "  Close");
  lv_obj_center(close_lbl);

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
