#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <LVGL_CYD.h>
#include <AsyncTCP.h>
#include <DCCExCS.h>
#include <Locos.h>
#include <Settings.h>
#include <WiFi.h>
#include <ThrottleServer.h>

#include "LVGL_Layouts.h"

#include "LocoUI.h"
#include "AccessoriesUI.h"
#include "PowerUI.h"
#include "SettingsUI.h"
#include "WiFiUI.h"
#include <memory>

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

LocoUI* locoUI;
AccessoriesUI* accUI;
PowerUI* pwrUI;
SettingsUI* setUI_ptr;

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

    float voltage = total / 10;
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
  SPIFFS.begin(true);

  // Initialize LVGL_CYD framework (handles Display, Touch, Backlight)
  LVGL_CYD::begin(USB_DOWN);

  setup_lvgl_layouts();

  locoUI = new LocoUI(dccExCS, locos, loco_tab);
  accUI = new AccessoriesUI(dccExCS, acc_tab);
  pwrUI = new PowerUI(dccExCS, power, pwr_tab);
  setUI_ptr = new SettingsUI(dccExCS, set_tab);

  // Load the settings
  Settings.load();

  apply_rotation();
  Settings.addEventListener(SettingsClass::Event::ROTATION_CHANGE, [](void*) {
      apply_rotation();
  });

  LVGL_CYD::backlight(Settings.brightness);
  Settings.addEventListener(SettingsClass::Event::BRIGHTNESS_CHANGE, [](void*) {
      LVGL_CYD::backlight(Settings.brightness);
  });

  // Setup the WiFi
  WiFi.setHostname(Settings.AP.SSID.c_str());
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

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
        csBuffer[csBufferLen++] = static_cast<uint8_t *>(data)[i];
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
