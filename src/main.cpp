#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <FileSystems.h>
#include <LVGL_CYD.h>
#include <WiFiClient.h>
#include <DCCEXProtocol.h>
#include <Locos.h>
#include <Settings.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
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
#include "AboutUI.h"
#include "utils/TeeStream.h"
#include "ProgramUI.h"
#include <memory>

// Touch and encoder pins are defined per-board in platformio.ini build_flags:
//   TOUCH_MOSI, TOUCH_MISO, TOUCH_CLK, TOUCH_CS, TOUCH_IRQ
//   ENCODER_CLK_PIN, ENCODER_DT_PIN, ENCODER_BTN_PIN

// ST7796 board: touch shares the LCD SPI bus (IO12/13/14). XPT2046_Bitbang reconfigures
// those pins as GPIO outputs which severs the HSPI hardware connection used by TFT_eSPI.
// Use TFT_eSPI's built-in getTouch() instead — it manages CS switching correctly.
#ifndef ST7796_DRIVER
XPT2046_Bitbang touchscreen(TOUCH_MOSI, TOUCH_MISO, TOUCH_CLK, TOUCH_CS, TOUCH_IRQ);
#endif

static void touch_read(lv_indev_t * indev, lv_indev_data_t * data) {
#ifdef ST7796_DRIVER
  // Lower pressure gate + multi-sample averaging to match the CYD's feel.
  // (The CYD detects via the XPT2046 IRQ line and median-filters 3 samples;
  // a single raw read behind a Z>300 gate felt insensitive by comparison.)
  if (LVGL_CYD::tft->getTouchRawZ() > 120) {
    uint32_t sx = 0, sy = 0;
    int n = 0;
    for (int s = 0; s < 3; s++) {
      uint16_t tx, ty;
      if (LVGL_CYD::tft->getTouchRaw(&tx, &ty)) { sx += tx; sy += ty; n++; }
    }
    if (n > 0) {
      int x = map(sx / n, Settings.TouchCal.xMin, Settings.TouchCal.xMax, 0, TFT_WIDTH  - 1);
      int y = map(sy / n, Settings.TouchCal.yMin, Settings.TouchCal.yMax, TFT_HEIGHT - 1, 0);
      data->point.x = constrain(x, 0, TFT_WIDTH  - 1);
      data->point.y = constrain(y, 0, TFT_HEIGHT - 1);
      data->state = LV_INDEV_STATE_PRESSED;
    } else {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
#else
  if (touchscreen.isTouched()) {
    Point p = touchscreen.getTouch();

    int x = map(p.x, Settings.TouchCal.xMin, Settings.TouchCal.xMax, TFT_WIDTH - 1, 0);
    x = constrain(x, 0, TFT_WIDTH - 1);

    int y = map(p.y, Settings.TouchCal.yMin, Settings.TouchCal.yMax, 0, TFT_HEIGHT - 1);
    y = constrain(y, 0, TFT_HEIGHT - 1);

    lv_disp_t * display = lv_disp_get_default();
    lv_display_rotation_t rotation = lv_display_get_rotation(display);

    if (rotation == LV_DISPLAY_ROTATION_90 || rotation == LV_DISPLAY_ROTATION_270) {
      x = TFT_WIDTH - x;
      y = TFT_HEIGHT - y;
    }

    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
#endif
}


const uint32_t POWER_CHECK = 60000 * 2;
const uint16_t CONNECTION_ALIVE_DELAY = 5000;

static void onCSCommand(const char* cmd) {
  ServerDescription desc;
  if (!parseServerDescription(cmd, desc)) return;
  csInfo.board  = desc.board;
  csInfo.shield = desc.shield;
  csInfo.build  = desc.build;
  Serial.printf("[DCC] CS Board: %s  Shield: %s  Build: %s\n",
                csInfo.board.c_str(), csInfo.shield.c_str(), csInfo.build.c_str());
}

WiFiClient csClient;
TeeStream  csMonitor(csClient, onCSCommand);
volatile bool csIsConnected = false;
volatile bool csConnectPending = false;
Locos locos;
ThrottleServer throttleServer;
AiEsp32RotaryEncoder rotaryEncoder(ENCODER_CLK_PIN, ENCODER_DT_PIN, ENCODER_BTN_PIN, -1, 2);
void IRAM_ATTR encoderISR()       { rotaryEncoder.readEncoder_ISR(); }
void IRAM_ATTR encoderButtonISR() { rotaryEncoder.readButton_ISR(); }
DNSServer dns;
bool ap_active = false;

std::unique_ptr<LocoUI> locoUI;
std::unique_ptr<AccessoriesUI> accUI;
std::unique_ptr<PowerUI> pwrUI;
std::unique_ptr<SettingsUI> setUI_ptr;

// Forward declarations
void build_ui_objects();
class AppDelegate;

// AppDelegate – receives callbacks from DCCEXProtocol and dispatches to UI
class AppDelegate : public DCCEXProtocolDelegate {
  // The library calls receivedIndividualTrackPower then receivedTrackPower for
  // MAIN-track changes. Guard against the spurious global call.
  bool _hadIndividualPowerUpdate = false;
public:
  void receivedServerVersion(int major, int minor, int patch) override {
    char vbuf[16]; snprintf(vbuf, sizeof(vbuf), "%d.%d.%d", major, minor, patch);
    csInfo.version = vbuf;
    Serial.printf("[DCC] CS Version: %s\n", csInfo.version.c_str());
  }

  void receivedMessage(const char* message) override {
    // CS message log goes to serial if needed for debugging
  }

  void receivedTrackPower(TrackPower state) override {
    // Library fires this for true global <p X> broadcasts AND for MAIN individual
    // changes (after receivedIndividualTrackPower). Skip the latter to avoid
    // incorrectly mirroring MAIN state onto PROG.
    if (_hadIndividualPowerUpdate) { _hadIndividualPowerUpdate = false; return; }
    bool on = (state == TrackPower::PowerOn);
    if (pwrUI) pwrUI->onPowerUpdate(on, on, false);
  }

  void receivedIndividualTrackPower(TrackPower state, int track) override {
    _hadIndividualPowerUpdate = true;
    if (pwrUI) pwrUI->onIndividualPowerUpdate(state, track);
  }

  void receivedLocoUpdate(Loco* loco) override {
    if (locoUI) locoUI->onLocoUpdate(loco);
  }

  void receivedCSConsist(int leadLoco, CSConsist* consist) override {
    if (locoUI) locoUI->onConsistUpdate(leadLoco, consist);
  }

  void receivedReadLoco(int address) override {
    if (setUI_ptr && setUI_ptr->getProgramUI())
      setUI_ptr->getProgramUI()->receivedReadLoco(address);
  }

  void receivedWriteLoco(int address) override {
    if (setUI_ptr && setUI_ptr->getProgramUI())
      setUI_ptr->getProgramUI()->receivedWriteLoco(address);
  }

  void receivedValidateCV(int cv, int value) override {
    if (setUI_ptr && setUI_ptr->getProgramUI())
      setUI_ptr->getProgramUI()->receivedReadCV(cv, value);
  }

  void receivedValidateCVBit(int cv, int bit, int value) override {
    if (setUI_ptr && setUI_ptr->getProgramUI())
      setUI_ptr->getProgramUI()->receivedReadCV(cv, value);
  }

  void receivedWriteCV(int cv, int value) override {
    if (setUI_ptr && setUI_ptr->getProgramUI())
      setUI_ptr->getProgramUI()->receivedWriteCV(cv, value);
  }

  void receivedTurnoutAction(int turnoutId, bool thrown) override {
    if (accUI) accUI->receivedTurnoutAction(turnoutId, thrown);
  }

  void receivedTurnoutList() override {
    if (accUI) accUI->receivedTurnoutList();
  }
};

AppDelegate appDelegate;
DCCEXProtocol dccexProtocol;

void build_ui_objects() {
  create_main_ui();
  locoUI    = std::make_unique<LocoUI>(dccexProtocol, locos, loco_tab);
  accUI     = std::make_unique<AccessoriesUI>(dccexProtocol, acc_tab);
  pwrUI     = std::make_unique<PowerUI>(dccexProtocol, pwr_tab);
  setUI_ptr = std::make_unique<SettingsUI>(dccexProtocol, set_tab);
}

void keepWiFiAlive(void *) {
  for (;;) {
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);

    if (Settings.CS.valid()) {
      if (!wifiConnected) {
        WiFi.begin(Settings.CS.SSID().c_str(), Settings.CS.password().c_str());
      } else if (!csIsConnected && !csClient.connected()) {
        csClient.setTimeout(3000);
        String csHost = Settings.CS.server();
        if (csHost.indexOf('.') == -1) csHost += ".local";
        if (csClient.connect(csHost.c_str(), Settings.CS.port())) {
          Serial.printf("[DCC] Command Station connected – %s:%u\n",
                        Settings.CS.server().c_str(), Settings.CS.port());
          csIsConnected = true;
          csInfo.connected = true;
          csConnectPending = true; // Signal loop() to call dccexProtocol.connect()
        }
      }
    }

    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        set_header_wifi_status(wifiConnected, wifiConnected ? WiFi.RSSI() : 0);
        xSemaphoreGive(lvgl_mutex);
    }

    vTaskDelay(CONNECTION_ALIVE_DELAY / portTICK_PERIOD_MS);
  }
}

void powerCheck(void *) {
  for (;;) {
    float total = 0;
    for (uint8_t i = 0; i < 10; i++) {
#ifdef ST7796_DRIVER
      // IO34 is GPI-only; analogReadMilliVolts handles ESP32 ADC non-linearity.
      // Board uses a 1:1 voltage divider (two equal resistors) so multiply by 2.
      total += analogReadMilliVolts(BATT_PIN) / 1000.0f * 2.0f;
#else
      total += touchscreen.readBattery();
#endif
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

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[Boot] Start: %u bytes free\n", ESP.getFreeHeap());

  ConfigFS.begin(true, "/config", 10, "config");
  WebsiteFS.begin(true, "/website", 10, "website");
  Serial.printf("[Boot] After FileSystems: %u bytes free\n", ESP.getFreeHeap());

  LVGL_CYD::begin(USB_DOWN);
  LVGL_CYD::backlight(0);
  Serial.printf("[Boot] After LVGL_CYD: %u bytes free\n", ESP.getFreeHeap());

#ifdef BOARD_HAS_PSRAM
  {
    // Replace the partial draw buffer with a full-screen framebuffer in PSRAM.
    // Frees LVGL from partial-strip rendering and keeps ~300 KB off the internal heap.
    const size_t psramBufSize = TFT_WIDTH * TFT_HEIGHT * (LV_COLOR_DEPTH / 8);
    void* psramBuf = ps_malloc(psramBufSize);
    if (psramBuf) {
      lv_display_set_buffers(lv_display_get_default(), psramBuf, nullptr,
                             psramBufSize, LV_DISPLAY_RENDER_MODE_FULL);
      Serial.printf("[Boot] LVGL framebuffer in PSRAM: %u bytes\n", psramBufSize);
    } else {
      Serial.println("[Boot] PSRAM framebuffer alloc failed, using library default");
    }
  }
#endif

#ifndef ST7796_DRIVER
  touchscreen.begin();
#endif

  lv_indev_t * indev = lv_indev_get_next(NULL);
  if (indev) lv_indev_set_read_cb(indev, touch_read);

  // Only mount the SD FAT driver when the saved storageMode requires it.
  // On first boot (no settings.json) we must also init so Settings::init()
  // can detect whether a card is present to choose the default mode.
  {
    bool needSD = !ConfigFS.exists("/settings.json");
    if (!needSD) {
      File f = ConfigFS.open("/settings.json");
      if (f) {
        StaticJsonDocument<16> filter;
        filter["storageMode"] = true;
        StaticJsonDocument<32> peek;
        deserializeJson(peek, f, DeserializationOption::Filter(filter));
        f.close();
        needSD = (peek["storageMode"] | 0) == SettingsClass::StorageMode::SD_CARD;
      }
    }
    if (needSD) {
      SPI.begin(18, 19, 23, 5);
      SD.begin(5, SPI, 4000000, "/sd", 5, true);
      Serial.printf("[Boot] After SD.begin: %u bytes free\n", ESP.getFreeHeap());
    } else {
      Serial.printf("[Boot] SD skipped (LittleFS mode): %u bytes free\n", ESP.getFreeHeap());
    }
  }

  lv_port_fs_init();

  Settings.load();
  Serial.printf("[Boot] After Settings.load: %u bytes free\n", ESP.getFreeHeap());

  apply_theme();
  Settings.addEventListener(SettingsClass::Event::THEME_CHANGE, [](void*) {
      apply_theme();

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
  lv_obj_set_style_image_recolor(splash_img, lv_color_make(50, 150, 255), 0);
  lv_obj_center(splash_img);

  lv_timer_handler();
  lv_obj_del(splash_img);

  LVGL_CYD::backlight(Settings.brightness);
  delay(2000);

  setup_lvgl_layouts();
  build_ui_objects();
  Serial.printf("[Boot] After UI setup: %u bytes free\n", ESP.getFreeHeap());

  Settings.addEventListener(SettingsClass::Event::THROTTLE_PROGRAM_ENTER, [](void*) {
      Serial.printf("[ThrottleProg] Entering – heap before: %u\n", ESP.getFreeHeap());

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

      Serial.printf("[ThrottleProg] Entered  – heap after : %u\n", ESP.getFreeHeap());
  });

  Settings.addEventListener(SettingsClass::Event::THROTTLE_PROGRAM_EXIT, [](void*) {
      Serial.printf("[ThrottleProg] Exiting – heap before: %u\n", ESP.getFreeHeap());
      build_ui_objects();
      Serial.printf("[ThrottleProg] Exited  – heap after : %u\n", ESP.getFreeHeap());
  });

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
      if (ap_active) dns.processNextRequest();
  }, 50, NULL);

  WiFi.onEvent(
      [](WiFiEvent_t event, WiFiEventInfo_t info) {
        Serial.printf("[WiFi] Connected – IP: %s  RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        MDNS.begin("dcc-ex-cyd");
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            set_header_wifi_status(true, WiFi.RSSI());
            xSemaphoreGive(lvgl_mutex);
        }
      },
      ARDUINO_EVENT_WIFI_STA_GOT_IP);

  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        Serial.printf("[WiFi] Disconnected\n");
        if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
            set_header_wifi_status(false, 0);
            xSemaphoreGive(lvgl_mutex);
        }
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  // Acquired loco count change
  locos.addEventListener(Locos::Event::COUNT_CHANGE, [](void *parameter) {
    auto count = *static_cast<uint8_t *>(parameter);
    if (xTaskGetCurrentTaskHandle() == xSemaphoreGetMutexHolder(lvgl_mutex)) {
        set_header_loco_count(count);
    } else if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
        set_header_loco_count(count);
        xSemaphoreGive(lvgl_mutex);
    }
  });

  Settings.addEventListener(SettingsClass::Event::CS_CHANGE,
                            [](void *) {
                              // Disconnect so keepWiFiAlive reconnects with new values
                              csClient.stop();
                              csIsConnected = false;
                              csInfo.connected = false;
                              WiFi.disconnect();
                            });

  // Rotary encoder
  rotaryEncoder.begin();
  rotaryEncoder.setup(encoderISR, encoderButtonISR);
  rotaryEncoder.setBoundaries(-10000, 10000, false);
  lv_timer_create([](lv_timer_t*) {
      static const uint16_t accelValues[] = { 0, 10, 20, 40 };
      static uint8_t lastAccel = 0xFF;
      if (Settings.LocoUI.acceleration != lastAccel) {
          lastAccel = Settings.LocoUI.acceleration;
          rotaryEncoder.setAcceleration(accelValues[lastAccel]);
      }

      long delta = rotaryEncoder.encoderChanged();
      if (delta != 0 && locoUI) {
          if (main_tabview) lv_tabview_set_act(main_tabview, 0, LV_ANIM_OFF);
          locoUI->nudgeSpeed((int)-delta * (1 << Settings.LocoUI.speedStep));
      }

      // Button long press – E-Stop (currently disabled)
      // static uint32_t btnDownSince = 0;
      // static bool btnFired = false;
      // if (rotaryEncoder.isEncoderButtonDown()) {
      //     if (btnDownSince == 0) btnDownSince = millis();
      //     else if (!btnFired && millis() - btnDownSince >= (uint32_t)Settings.emergencyStopDelay * 1000U) {
      //         dccexProtocol.emergencyStop();
      //         Serial.printf("[DCC] Emergency stop triggered\n");
      //         btnFired = true;
      //     }
      // } else {
      //     btnDownSince = 0;
      //     btnFired = false;
      // }
  }, 20, nullptr);

  // Set delegate before connecting
  dccexProtocol.setDelegate(&appDelegate);

  xTaskCreatePinnedToCore(powerCheck, "powerCheck", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(keepWiFiAlive, "keepWiFiAlive", 4096, NULL, 1, NULL, 1);

  throttleServer.begin();
  Serial.printf("[Boot] After WebServer: %u bytes free (min ever: %u)\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
}

void loop() {
  // Pick up CS connection established by keepWiFiAlive
  if (csConnectPending) {
    csConnectPending = false;
    dccexProtocol.connect(&csMonitor);
    dccexProtocol.sendCommand("s"); // Request server version/status
    if (xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
      set_header_cs_status(true);
      xSemaphoreGive(lvgl_mutex);
    }
  }

  if (lvgl_mutex && xSemaphoreTake(lvgl_mutex, portMAX_DELAY) == pdTRUE) {
    // Detect CS disconnect
    if (csIsConnected && !csClient.connected()) {
      csIsConnected = false;
      csInfo.connected = false;
      Serial.printf("[DCC] Command Station disconnected\n");
      set_header_cs_status(false);
    }

    // Process CS protocol (delegate callbacks fire here, already holding mutex)
    if (csIsConnected) {
      dccexProtocol.check();
    }

    lv_timer_handler();
    xSemaphoreGive(lvgl_mutex);
  }
  delay(5);
}
