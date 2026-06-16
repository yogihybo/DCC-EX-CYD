#pragma once

#include <LVGL_CYD.h>
#include <Settings.h>
#include <WiFi.h>
#include <DNSServer.h>

#include "LVGL_Layouts.h"

class WiFiUI : public UIView {
  private:
    lv_obj_t* _container;
    lv_obj_t* _labelIP;
    lv_obj_t* _textareaSSID;
    lv_obj_t* _textareaPassword;
    lv_obj_t* _textareaServer;
    lv_obj_t* _textareaPort;
    lv_obj_t* _qr;
    
    lv_obj_t* _keyboard;

    wifi_event_id_t _ipGotHandler;
    wifi_event_id_t _ipDisconnectedHandler;

    static void ta_event_cb(lv_event_t * e);
    static void kb_event_cb(lv_event_t * e);
    static void close_btn_event_cb(lv_event_t * e);

  public:
    WiFiUI(lv_obj_t* parent);
    ~WiFiUI() override;

    lv_obj_t* getContainer() { return _container; }
};
