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

#include "LVGL_Layouts.h"

const uint32_t POWER_CHECK = 60000 * 2; // 2 Minutes
const uint8_t BATTERY_PIN = 34;
const uint16_t CONNECTION_ALIVE_DELAY = 5000;

AsyncClient csClient;
uint8_t csBuffer[1024];
uint16_t csBufferLen = 0;
DCCExCS dccExCS(csClient);
DCCExCS::Power power;
Locos locos;

void keepWiFiAlive(void *) {
  for (;;) {
    if (Settings.CS.valid()) { // Valid if we have SSID, Server & Port
      if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(Settings.CS.SSID().c_str(), Settings.CS.password().c_str());
      }
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

    set_header_power_status(voltage);
    vTaskDelay(POWER_CHECK / portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);

  // Start file systems
  SPIFFS.begin(true);

  // Initialize LVGL_CYD framework (handles Display, Touch, Backlight)
  LVGL_CYD::begin(USB_LEFT);

  setup_lvgl_layouts();

  // Load the settings
  Settings.load();

  // Setup the WiFi
  WiFi.setHostname(Settings.AP.SSID.c_str());
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // WiFi connected
  WiFi.onEvent(
      [](WiFiEvent_t event, WiFiEventInfo_t info) {
        set_header_wifi_status(true);
        connectToCS();
      },
      ARDUINO_EVENT_WIFI_STA_GOT_IP);
  // WiFi disconnected
  WiFi.onEvent([](WiFiEvent_t event,
                  WiFiEventInfo_t info) { set_header_wifi_status(false); },
               ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  // CS connected
  csClient.onConnect([](void *arg, AsyncClient *client) {
    set_header_cs_status(true);
    dccExCS.getCSPower(); // Get current power status
  });
  // CS disconnected
  csClient.onDisconnect([](void *arg, AsyncClient *client) {
    set_header_cs_status(false);
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
    set_header_loco_count(count);
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

  xTaskCreatePinnedToCore(powerCheck, "powerCheck", 1024, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(keepWiFiAlive, "keepWiFiAlive", 2048, NULL, 1, NULL, 1);
}

void loop() {
  lv_timer_handler();
  delay(5);
}
