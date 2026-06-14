#pragma once

#include <LVGL_CYD.h>
#include <Settings.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
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

    DNSServer dns;
    AsyncWebServer server;

    wifi_event_id_t _ipGotHandler;
    wifi_event_id_t _ipDisconnectedHandler;
    uint8_t _updatedHandler;

    static void ta_event_cb(lv_event_t * e);
    static void kb_event_cb(lv_event_t * e);
    static void close_btn_event_cb(lv_event_t * e);
    static void loop_timer_cb(lv_timer_t* timer);

    lv_timer_t* _loop_timer;

  public:
    WiFiUI(lv_obj_t* parent);
    ~WiFiUI() override;

    lv_obj_t* getContainer() { return _container; }
};
