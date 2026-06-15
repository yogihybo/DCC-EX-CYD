#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <FileSystems.h>
#include <LVGL_CYD.h>
#include <AsyncTCP.h>
#include <DCCExCS.h>
#include <Locos.h>
#include <Settings.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ThrottleServer.h>
#include "XPT2046_Bitbang.h"

#include "LVGL_Layouts.h"
#include "lv_port_fs.h"

#include "logo.h"
#include "LocoUI.h"
#include "AccessoriesUI.h"
#include "PowerUI.h"
#include "SettingsUI.h"
#include "WiFiUI.h"
#include <memory>

#define MOSI_PIN 32
#define MISO_PIN 39
#define CLK_PIN  25
#define CS_PIN   33
#define IRQ_PIN  36

XPT2046_Bitbang touchscreen(MOSI_PIN, MISO_PIN, CLK_PIN, CS_PIN, IRQ_PIN);

static void touch_read(lv_indev_t * indev, lv_indev_data_t * data) {
  if (touchscreen.isTouched()) {
    Point p = touchscreen.getTouch();
    
    // Map using calibration data from Settings
    // p.x (CMD 0x90) maps to the SHORT edge (239-0)
    int x = map(p.x, Settings.TouchCal.xMin, Settings.TouchCal.xMax, 239, 0);
    x = constrain(x, 0, 239);

    // p.y (CMD 0xD0) maps to the LONG edge (0-319)
    int y = map(p.y, Settings.TouchCal.yMin, Settings.TouchCal.yMax, 0, 319);
    y = constrain(y, 0, 319);

    // Since we are using the bitbang library and bypassing LVGL_CYD's touch driver,
    // we must replicate LVGL_CYD's specific orientation fix.
    lv_disp_t * display = lv_disp_get_default();
    lv_display_rotation_t rotation = lv_display_get_rotation(display);
    
    if (rotation == LV_DISPLAY_ROTATION_90 || rotation == LV_DISPLAY_ROTATION_270) {
      x = 240 - x;
      y = 320 - y;
    } 

    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}


const uint32_t POWER_CHECK = 60000 * 2; // 2 Minutes
const uint8_t BATTERY_PIN = 34;
const uint16_t CONNECTION_ALIVE_DELAY = 5000;

AsyncClient csClient;
uint8_t csBuffer[1024];
uint16_t csBufferLen = 0;
DCCExCS dccExCS(csClient);
DCCExCS::Power power;
Locos locos;
ThrottleServer throttleServer;
DNSServer dns;
bool ap_active = false;

std::unique_ptr<LocoUI> locoUI;
std::unique_ptr<AccessoriesUI> accUI;
std::unique_ptr<PowerUI> pwrUI;
std::unique_ptr<SettingsUI> setUI_ptr;

void keepWiFiAlive(void *) {
  for (;;) {
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (Settings.CS.valid()) { // Valid if we have SSID, Server & Port
      if (!connected) {
        WiFi.begin(Settings.CS.SSID().c_str(), Settings.CS.password().c_str());
      }
    }
    
    // Dynamically update WiFi UI with RSSI
    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        set_header_wifi_status(connected, connected ? WiFi.RSSI() : 0);
        xSemaphoreGive(lvgl_mutex);
    }
    
    vTaskDelay(CONNECTION_ALIVE_DELAY / portTICK_PERIOD_MS);
  }
}

void connectToCS() {
  if (WiFi.status() == WL_CONNECTED) {
    csClient.connect(Settings.CS.server().c_str(), Settings.CS.port());
  }
}

void powerCheck(void *) {
  for (;;) {
    uint32_t total = 0;
    for (uint8_t i = 0; i < 10; i++) {
      total += analogReadMilliVolts(BATTERY_PIN);
      delay(100);
    }

    float voltage = total / 10.0f;
    voltage *= 2;
    voltage /= 1000;

    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        set_header_power_status(voltage);
        xSemaphoreGive(lvgl_mutex);
    }
    vTaskDelay(POWER_CHECK / portTICK_PERIOD_MS);
  }
}

void apply_rotation() {
  lv_display_rotation_t rot = USB_DOWN;
  if (Settings.rotation == SettingsClass::Rotation::INVERTED) rot = USB_UP;
  lv_display_set_rotation(lv_display_get_default(), rot);
}

void setup() {
  Serial.begin(115200);

  // Start file systems
  ConfigFS.begin(true, "/config", 10, "config");
  WebsiteFS.begin(true, "/website", 10, "website");

  // Initialize LVGL_CYD framework (handles Display, Touch, Backlight)
  LVGL_CYD::begin(USB_DOWN);
  LVGL_CYD::backlight(0); // Immediately turn off backlight to hide the white startup screen flash
  
  // Initialize bit-bang touch screen
  touchscreen.begin();

  // Find the LVGL input device created by LVGL_CYD and overwrite its read_cb
  // This effectively bypasses LVGL_CYD's hardware SPI touch driver and uses our
  // bit-banged touch reader instead, freeing up the VSPI hardware for the SD card.
  lv_indev_t * indev = lv_indev_get_next(NULL);
  if (indev) {
    lv_indev_set_read_cb(indev, touch_read);
  }

  // Initialize the SD card using default VSPI pins
  SPI.begin(18, 19, 23, 5);
  SD.begin(5, SPI, 4000000, "/sd", 5, true);

  // Initialize LVGL file system driver
  lv_port_fs_init();

  // Load the settings
  Settings.load();

  apply_theme();
  Settings.addEventListener(SettingsClass::Event::THEME_CHANGE, [](void*) {
      apply_theme();
  });

  apply_rotation();
  Settings.addEventListener(SettingsClass::Event::ROTATION_CHANGE, [](void*) {
      apply_rotation();
  });

  Settings.addEventListener(SettingsClass::Event::BRIGHTNESS_CHANGE, [](void*) {
      LVGL_CYD::backlight(Settings.brightness);
  });

  // --- SPLASH SCREEN ---
  lv_obj_t* splash_img = lv_image_create(lv_scr_act());
  lv_image_set_src(splash_img, &logo);
  lv_obj_set_style_image_recolor_opa(splash_img, LV_OPA_COVER, 0);
  lv_obj_set_style_image_recolor(splash_img, lv_color_make(50, 150, 255), 0); // DCC-EX-CYD blue theme color
  lv_obj_center(splash_img);
  
  lv_timer_handler(); // Render splash screen
  LVGL_CYD::backlight(Settings.brightness); // Fade in / turn on backlight now that the frame is ready
  delay(2000);
  
  lv_obj_del(splash_img);

  setup_lvgl_layouts();

  locoUI = std::make_unique<LocoUI>(dccExCS, locos, loco_tab);
  accUI = std::make_unique<AccessoriesUI>(dccExCS, acc_tab);
  pwrUI = std::make_unique<PowerUI>(dccExCS, power, pwr_tab);
  setUI_ptr = std::make_unique<SettingsUI>(dccExCS, set_tab);

  // Setup the WiFi
  WiFi.setHostname(Settings.AP.SSID.c_str());
  WiFi.persistent(false);
  
  if (Settings.AP.enabled) {
      WiFi.mode(WIFI_AP_STA);
      WiFi.softAP(Settings.AP.SSID.c_str(), Settings.AP.password.c_str());
      dns.start(53, Settings.AP.SSID.c_str(), WiFi.softAPIP());
      ap_active = true;
  } else {
      WiFi.mode(WIFI_STA);
  }
  
  WiFi.setSleep(false);
  
  Settings.addEventListener(SettingsClass::Event::CS_CHANGE, [](void*) {
      if (Settings.AP.enabled && !ap_active) {
          bool connected = (WiFi.status() == WL_CONNECTED);
          WiFi.mode(connected ? WIFI_AP_STA : WIFI_AP);
          WiFi.softAP(Settings.AP.SSID.c_str(), Settings.AP.password.c_str());
          dns.start(53, Settings.AP.SSID.c_str(), WiFi.softAPIP());
          ap_active = true;
      } else if (!Settings.AP.enabled && ap_active) {
          dns.stop();
          WiFi.mode(WIFI_STA);
          ap_active = false;
      }
  });

  lv_timer_create([](lv_timer_t* t) {
      if (ap_active) {
          dns.processNextRequest();
      }
  }, 50, NULL);

  // WiFi connected
  WiFi.onEvent(
      [](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            set_header_wifi_status(true, WiFi.RSSI());
            xSemaphoreGive(lvgl_mutex);
        }
        connectToCS();
      },
      ARDUINO_EVENT_WIFI_STA_GOT_IP);
  // WiFi disconnected
  WiFi.onEvent([](WiFiEvent_t event,
                  WiFiEventInfo_t info) { 
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            set_header_wifi_status(false, 0); 
            xSemaphoreGive(lvgl_mutex);
        }
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  // CS connected
  csClient.onConnect([](void *arg, AsyncClient *client) {
    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        set_header_cs_status(true);
        xSemaphoreGive(lvgl_mutex);
    }
    dccExCS.getCSPower(); // Get current power status
  });
  // CS disconnected
  csClient.onDisconnect([](void *arg, AsyncClient *client) {
    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        set_header_cs_status(false);
        xSemaphoreGive(lvgl_mutex);
    }
    connectToCS();
  });
  // If the connection to the CS times out then close it so it'll attempt a reconnect
  csClient.onTimeout(
      [](void *arg, AsyncClient *client, uint32_t time) { csClient.close(); });
  // CS data
  csClient.onData([](void *arg, AsyncClient *client, void *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
      if (static_cast<uint8_t *>(data)[i] == '<') { // Start of cmd
        csBuffer[0] = '\0';
        csBufferLen = 0;
      } else if (static_cast<uint8_t *>(data)[i] ==
                 '>') { // End of cmd, pass onto ex cs handler
        csBuffer[csBufferLen] = '\0';
        dccExCS.handleCS(csBuffer, csBufferLen);
      } else {
        if (csBufferLen < sizeof(csBuffer) - 1) {
          csBuffer[csBufferLen++] = static_cast<uint8_t *>(data)[i];
        }
      }
    }
  });

  // Aquired loco count change
  locos.addEventListener(Locos::Event::COUNT_CHANGE, [](void *parameter) {
    auto count = *static_cast<uint8_t *>(parameter);
    if (xTaskGetCurrentTaskHandle() == xSemaphoreGetMutexHolder(lvgl_mutex)) {
        set_header_loco_count(count);
    } else if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        set_header_loco_count(count);
        xSemaphoreGive(lvgl_mutex);
    }
  });

  // CS Power event
  dccExCS.addEventListener(DCCExCS::Event::BROADCAST_POWER,
                           [](void *parameter) {
                             power = *static_cast<DCCExCS::Power *>(parameter); // Remember current power states
                           });

  // If the CS settings change we disconnect so the keep alive task will
  // reconnect using new values
  Settings.addEventListener(SettingsClass::Event::CS_CHANGE,
                            [](void *) { WiFi.disconnect(); });

  xTaskCreatePinnedToCore(powerCheck, "powerCheck", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(keepWiFiAlive, "keepWiFiAlive", 2048, NULL, 1, NULL, 1);

  throttleServer.begin();
}

void loop() {
  if (lvgl_mutex && xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
      lv_timer_handler();
      xSemaphoreGive(lvgl_mutex);
  }
  delay(5);
}
