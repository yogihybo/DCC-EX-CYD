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
#include <AiEsp32RotaryEncoder.h>

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

#define ENCODER_CLK_PIN 22  // GPIO22 – rotary CLK (A); TEMPORARY for testing
#define ENCODER_DT_PIN  27  // GPIO27 – rotary DT  (B); HW-040 supplies pull-up
#define ENCODER_BTN_PIN 35  // GPIO35 – encoder push button; TEMPORARY for testing (input-only, no internal pull-up)

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
const uint16_t CONNECTION_ALIVE_DELAY = 5000;

AsyncClient csClient;
volatile bool csIsConnected = false;
uint8_t csBuffer[256]; // DCC-EX commands are short (<20 bytes typically)
uint16_t csBufferLen = 0;
DCCExCS dccExCS(csClient);
DCCExCS::Power power;
Locos locos;
ThrottleServer throttleServer;
AiEsp32RotaryEncoder rotaryEncoder(ENCODER_CLK_PIN, ENCODER_DT_PIN, ENCODER_BTN_PIN, -1, 2); // -1 = no VCC pin; 2 steps per detent (HW-040)
void IRAM_ATTR encoderISR()       { rotaryEncoder.readEncoder_ISR(); }
void IRAM_ATTR encoderButtonISR() { rotaryEncoder.readButton_ISR(); }
DNSServer dns;
bool ap_active = false;

std::unique_ptr<LocoUI> locoUI;
std::unique_ptr<AccessoriesUI> accUI;
std::unique_ptr<PowerUI> pwrUI;
std::unique_ptr<SettingsUI> setUI_ptr;

// Forward declaration – defined below setup()
void build_ui_objects();

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
    float total = 0;
    for (uint8_t i = 0; i < 10; i++) {
      total += touchscreen.readBattery();
      delay(100);
    }

    float voltage = total / 10.0f;

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

// Build the tabview and all UI objects.
// Called at boot and again on THROTTLE_PROGRAM_EXIT.
void build_ui_objects() {
  create_main_ui();
  locoUI    = std::make_unique<LocoUI>(dccExCS, locos, loco_tab);
  accUI     = std::make_unique<AccessoriesUI>(dccExCS, acc_tab);
  pwrUI     = std::make_unique<PowerUI>(dccExCS, power, pwr_tab);
  setUI_ptr = std::make_unique<SettingsUI>(dccExCS, set_tab);
}

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[Boot] Start: %u bytes free\n", ESP.getFreeHeap());

  // Start file systems
  ConfigFS.begin(true, "/config", 10, "config");
  WebsiteFS.begin(true, "/website", 10, "website");
  Serial.printf("[Boot] After FileSystems: %u bytes free\n", ESP.getFreeHeap());

  // Initialize LVGL_CYD framework (handles Display, Touch, Backlight)
  LVGL_CYD::begin(USB_DOWN);
  LVGL_CYD::backlight(0); // Immediately turn off backlight to hide the white startup screen flash
  Serial.printf("[Boot] After LVGL_CYD: %u bytes free\n", ESP.getFreeHeap());
  
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
  Serial.printf("[Boot] After SD.begin: %u bytes free\n", ESP.getFreeHeap());

  // Initialize LVGL file system driver
  lv_port_fs_init();

  // Load the settings
  Settings.load();
  Serial.printf("[Boot] After Settings.load: %u bytes free\n", ESP.getFreeHeap());

  apply_theme();
  Settings.addEventListener(SettingsClass::Event::THEME_CHANGE, [](void*) {
      apply_theme();

      // Tear down existing UI — LVGL themes only apply at object creation time,
      // so all widgets must be destroyed and rebuilt to pick up the new theme.
      lv_obj_t* tv = main_tabview;
      main_tabview = nullptr;
      loco_tab     = nullptr;
      acc_tab      = nullptr;
      pwr_tab      = nullptr;
      set_tab      = nullptr;

      setUI_ptr.reset();
      pwrUI.reset();
      accUI.reset();
      locoUI.reset();
      if (tv) lv_obj_del(tv);

      rebuild_header_bar();
      build_ui_objects();

      // Re-apply current status to the freshly created widgets
      set_header_wifi_status(WiFi.status() == WL_CONNECTED, WiFi.RSSI());
      set_header_cs_status(csIsConnected);
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

  lv_obj_del(splash_img); // delete the splash screen

  LVGL_CYD::backlight(Settings.brightness); // Fade in / turn on backlight now that the frame is ready
  delay(2000);
  
  // Create the header bar (once only)
  setup_lvgl_layouts();

  // Build tabview + all UI objects for the first time
  build_ui_objects();
  Serial.printf("[Boot] After UI setup: %u bytes free\n", ESP.getFreeHeap());

  // -------------------------------------------------------
  // Throttle Programming Mode – tear down / rebuild UI
  // -------------------------------------------------------

  // THROTTLE_PROGRAM_ENTER: destroy all UI objects so the web server has
  // maximum free heap. The tabview was already deleted inside
  // throttle_programming_event_cb via lv_obj_del; here we just reset the
  // C++ unique_ptrs to call their destructors and free C++ heap.
  Settings.addEventListener(SettingsClass::Event::THROTTLE_PROGRAM_ENTER, [](void*) {
      Serial.printf("[ThrottleProg] Entering – heap before: %u\n", ESP.getFreeHeap());

      // Save the raw LVGL pointer before nulling the global – we need it to
      // actually delete the widget after the C++ destructors have run.
      lv_obj_t* tv = main_tabview;

      // Null all global tab pointers so nothing holds a stale reference.
      main_tabview = nullptr;
      loco_tab     = nullptr;
      acc_tab      = nullptr;
      pwr_tab      = nullptr;
      set_tab      = nullptr;

      // Reset C++ UI objects first – their destructors call lv_obj_del on their
      // own containers (children of the tabs), leaving the tabview itself empty.
      setUI_ptr.reset();
      pwrUI.reset();
      accUI.reset();
      locoUI.reset();

      // Now it is safe to delete the tabview LVGL widget: its tab content has
      // already been removed by the destructors above. Without this call the
      // old tabview stays rendered on screen and a second one is created on top
      // of it when the UI is rebuilt after exiting programming mode.
      if (tv) lv_obj_del(tv);

      Serial.printf("[ThrottleProg] Entered  – heap after : %u\n", ESP.getFreeHeap());
  });

  // THROTTLE_PROGRAM_EXIT: rebuild the full UI from scratch.
  Settings.addEventListener(SettingsClass::Event::THROTTLE_PROGRAM_EXIT, [](void*) {
      Serial.printf("[ThrottleProg] Exiting – heap before: %u\n", ESP.getFreeHeap());
      build_ui_objects();
      Serial.printf("[ThrottleProg] Exited  – heap after : %u\n", ESP.getFreeHeap());
  });

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
        Serial.printf("[WiFi] Connected – IP: %s  RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
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
        Serial.printf("[WiFi] Disconnected\n");
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            set_header_wifi_status(false, 0);
            xSemaphoreGive(lvgl_mutex);
        }
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  // CS connected
  csClient.onConnect([](void *arg, AsyncClient *client) {
    Serial.printf("[DCC] Command Station connected – %s:%u\n",
                  Settings.CS.server().c_str(), Settings.CS.port());
    csIsConnected = true;
    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        set_header_cs_status(true);
        xSemaphoreGive(lvgl_mutex);
    }
    dccExCS.getCSPower(); // Get current power status
  });
  // CS disconnected
  csClient.onDisconnect([](void *arg, AsyncClient *client) {
    Serial.printf("[DCC] Command Station disconnected\n");
    csIsConnected = false;
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

  // Acquired loco count change
  static uint8_t lastLocoCount = 0;
  locos.addEventListener(Locos::Event::COUNT_CHANGE, [](void *parameter) {
    auto count = *static_cast<uint8_t *>(parameter);
    if (count > lastLocoCount) {
      Serial.printf("[Loco] Added   – total: %u\n", count);
    } else if (count < lastLocoCount) {
      Serial.printf("[Loco] Removed – total: %u\n", count);
    }
    lastLocoCount = count;
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

  // Rotary encoder – AiEsp32RotaryEncoder (polling via LVGL timer, ISR-safe)
  rotaryEncoder.begin();
  rotaryEncoder.setup(encoderISR, encoderButtonISR);
  rotaryEncoder.setBoundaries(-10000, 10000, false);
  lv_timer_create([](lv_timer_t*) {
      // Sync acceleration setting live
      static const uint16_t accelValues[] = { 0, 10, 20, 40 };
      static uint8_t lastAccel = 0xFF;
      if (Settings.LocoUI.acceleration != lastAccel) {
          lastAccel = Settings.LocoUI.acceleration;
          rotaryEncoder.setAcceleration(accelValues[lastAccel]);
      }

      // Rotation
      long delta = rotaryEncoder.encoderChanged();
      if (delta != 0 && locoUI)
          locoUI->nudgeSpeed((int)-delta * (1 << Settings.LocoUI.speedStep));

      // Button – long press triggers E-Stop
      static uint32_t btnDownSince = 0;
      static bool btnFired = false;
      if (rotaryEncoder.isEncoderButtonDown()) {
          if (btnDownSince == 0) btnDownSince = millis();
          else if (!btnFired && millis() - btnDownSince >= (uint32_t)Settings.emergencyStopDelay * 1000U) {
              dccExCS.emergencyStopAll();
              Serial.printf("[DCC] Emergency stop triggered\n");
              btnFired = true;
          }
      } else {
          btnDownSince = 0;
          btnFired = false;
      }
  }, 20, nullptr);

  xTaskCreatePinnedToCore(powerCheck, "powerCheck", 2048, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(keepWiFiAlive, "keepWiFiAlive", 4096, NULL, 1, NULL, 1);

  throttleServer.begin();
  Serial.printf("[Boot] After WebServer: %u bytes free (min ever: %u)\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

void loop() {
  if (lvgl_mutex && xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
      lv_timer_handler();
      xSemaphoreGive(lvgl_mutex);
  }
  delay(5);
}
