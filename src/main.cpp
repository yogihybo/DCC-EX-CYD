#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>

#include <XPT2046_Touchscreen.h>
#include <DFRobot_LIS2DW12.h>
#undef ERR_OK // Needed as the DFRobot_LIS2DW12.h header has an unused define
              // that conflicts
#include <AboutUI.h>
#include <AccessoriesUI.h>
#include <AsyncTCP.h>
#include <Children/Keypad.h>
#include <Children/MessageBox.h>
#include <Children/Pin.h>
#include <Components/BottomBar.h>
#include <DCCExCS.h>
#include <LocoByNameUI.h>
#include <LocoUI.h>
#include <Locos.h>
#include <PowerUI.h>
#include <ProgramUI.h>
#include <SDCardUI.h>
#include <Settings.h>
#include <SettingsUI.h>
#include <UI.h>
#include <UIHeader.h>
#include <WiFi.h>
#include <WiFiUI.h>

const uint32_t POWER_CHECK = 60000 * 2; // 2 Minutes
const uint8_t BATTERY_PIN = 34;
const uint8_t GESTURE_DISTANCE = 15;
const uint16_t CONNECTION_ALIVE_DELAY = 5000;



TFT_eSPI tft;
SPIClass mySpi(VSPI);
XPT2046_Touchscreen ts(33);

struct {
  bool enabled = false;
  DFRobot_IIS2DLPC_I2C inst;
  DFRobot_LIS2DW12::eOrient_t last = DFRobot_LIS2DW12::eOrient_t::eYDown;
} acce;

bool isTouched() { return ts.touched(); }
uint8_t getTouchPoints(TouchPoint *points) {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    uint16_t px = map(p.x, 200, 3700, 0, 240);
    uint16_t py = map(p.y, 240, 3800, 0, 320);
    points[0].x = constrain(px, 0, 240);
    points[0].y = constrain(py, 0, 320);
    return 1;
  }
  return 0;
}



AsyncClient csClient;
uint8_t csBuffer[1024];
uint16_t csBufferLen = 0;
DCCExCS dccExCS(csClient);
DCCExCS::Power power;
Locos locos;

std::unique_ptr<UIHeader> uiHeader;
std::unique_ptr<UI> activeUI;
std::unique_ptr<BottomBar> bottomBar;
std::function<std::unique_ptr<UI>()> setUI;

void setLocoUI();
void setSettingsUI();
void setAccessoriesUI();
void setPowerUI();

void setPinUI(uint32_t pin, const Events::EventCallback &&onValid) {
  if (!pin) {
    onValid(nullptr);
  } else {
    setUI = [pin, onValid] {
      auto ui = std::make_unique<Pin>(pin);
      ui->addEventListener(Pin::Event::VALID, std::move(onValid));
      ui->addEventListener(Pin::Event::CANCEL, [](void *) { setSettingsUI(); });
      return ui;
    };
  }
}

void setLocoKeypadUI(const Events::EventCallback &&onCancel) {
  setUI = [onCancel] {
    auto ui = std::make_unique<Keypad>("Loco Address", 10293, 0);
    ui->addEventListener(Keypad::Event::ENTER, [](void *parameter) {
      locos.add(*static_cast<uint16_t *>(parameter));
      setLocoUI();
    });
    ui->addEventListener(Keypad::Event::CANCEL, std::move(onCancel));
    return ui;
  };
}

void setLocoByNameUI(bool group) {
  setUI = [group] {
    auto ui = std::make_unique<LocoByNameUI>(group);
    ui->addEventListener(LocoByNameUI::Event::SELECTED, [](void *parameter) {
      locos.add(*static_cast<uint16_t *>(parameter));
      setLocoUI();
    });
    return ui;
  };
}

void setLocoUI() {
  setUI = [] {
    auto ui = std::make_unique<LocoUI>(dccExCS, locos);
    ui->addEventListener(LocoUI::Event::SWIPE_ACTION, [](void *parameter) {
      using Action = SettingsClass::LocoUI::Swipe::Action;
      switch (const auto action = *static_cast<uint8_t *>(parameter)) {
      case Action::KEYPAD: {
        setLocoKeypadUI([](void *) { setLocoUI(); });
      } break;
      case Action::NAME:
      case Action::GROUP: {
        bool group = action == Action::GROUP;
        setLocoByNameUI(group);
      } break;
      case Action::RELEASE: {
        dccExCS.releaseLoco(locos.remove());
        if ((Settings.LocoUI.Swipe.release == Action::NEXT ||
             Settings.LocoUI.Swipe.release == Action::PREV) &&
            locos == 0) {
          setLocoUI();
        } else {
          (static_cast<LocoUI *>(activeUI.get()))
              ->dispatchEvent(LocoUI::Event::SWIPE_ACTION,
                              &Settings.LocoUI.Swipe.release);
        }
      } break;
      case Action::NEXT: {
        locos.next();
        setLocoUI();
      } break;
      case Action::PREV: {
        locos.prev();
        setLocoUI();
      } break;
      }
    });

    return ui;
  };
}

void setSettingsUI() {
  setPinUI(Settings.pin, [](void *) {
    setUI = [] {
      auto ui = std::make_unique<SettingsUI>();
      ui->addEventListener(SettingsUI::Event::WIFI, [](void *) {
        setPinUI(Settings.pin, [](void *) {
          setUI = [] { return std::make_unique<WiFiUI>(); };
        });
      });
      ui->addEventListener(SettingsUI::Event::ABOUT, [](void *) {
        setUI = [] { return std::make_unique<AboutUI>(dccExCS); };
      });
      return ui;
    };
  });
}

void setAccessoriesUI() {
  setUI = [] { return std::make_unique<AccessoriesUI>(dccExCS); };
}

void setPowerUI() {
  setUI = [] { return std::make_unique<PowerUI>(dccExCS, power); };
}



// Based off the helpful blog post here,
// https://savjee.be/2020/02/esp32-keep-wifi-alive-with-freertos-task/
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

    uiHeader->setPowerStatus(voltage);
    vTaskDelay(POWER_CHECK / portTICK_PERIOD_MS);
  }
}

void setRotation() {
  bool standard = Settings.rotation == SettingsClass::Rotation::STANDARD;
  if (Settings.rotation == SettingsClass::Rotation::ACCELEROMETER) {
    standard =
        acce.enabled ? acce.last == DFRobot_LIS2DW12::eOrient_t::eYDown : true;
  }

  UI::tft->setRotation(standard ? 2 : 0);
  ts.setRotation(standard ? 2 : 0);

  if (activeUI != nullptr) {
    UI::tft->fillScreen(UI::COLOR_MAIN_BG);
    uiHeader->handleRedraw();
    activeUI->handleRedraw();
  }
}

void setup() {
  Serial.begin(115200);

  // Start file systems
  SPIFFS.begin(true);
  pinMode(5, OUTPUT);
  digitalWrite(5, HIGH);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  pinMode(33, OUTPUT);
  digitalWrite(33, HIGH);
#define TFT_BL 21

  // Set the pin as an output and pull it HIGH to turn on the screen
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Now initialize the display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  // pinMode(TFT_BL, OUTPUT);
  // analogWrite(TFT_BL, 0);
  // pinMode(TFT_RST, OUTPUT);
  // TFT_SET_BL(100); // full brightness

  // UI::setBacklight(100); // 100% brightness


  // Start the TFT display
  UI::tft = &tft;
  UI::tft->init();
  // UI::tft->fillScreen(UI::COLOR_MAIN_BG);
  // UI::tft->setTextColor(TFT_WHITE, UI::COLOR_MAIN_BG);
  // UI::tft->loadFont("/fonts/SegoeUI-20");

  // pinMode(12, OUTPUT);
  // digitalWrite(12, HIGH); // Keep backlight on
  // digitalWrite(12, LOW); // Turn off backlight
  // TODO, screen standby? can it remember buffer

  // Start the touchscreen
  mySpi.begin(25, 39, 32, 33);
  ts.begin(mySpi);

  // Start the accelerometer
  if (acce.inst.begin()) {
    acce.inst.softReset();
    acce.inst.setRange(DFRobot_LIS2DW12::e2_g);
    acce.inst.setPowerMode(DFRobot_LIS2DW12::eContLowPwrLowNoise1_12bit);
    acce.inst.setDataRate(DFRobot_LIS2DW12::eRate_200hz);
    acce.inst.set6DThreshold(DFRobot_LIS2DW12::eDegrees60);
    acce.inst.setInt1Event(DFRobot_LIS2DW12::e6D);
    delay(25);
    acce.enabled = true;
    acce.last = acce.inst.getOrientation();
  }



  // Unable to mount SD card - now optional
  if (SD.cardType() == CARD_NONE) {
    Serial.println("SD card not detected, running from SPIFFS only.");
  }

  // Load the settings
  Settings.load();

  // Set initial rotation
  setRotation();

  // Rotation settings change
  Settings.addEventListener(SettingsClass::Event::ROTATION_CHANGE, [](void *) {
    if (acce.enabled) {
      acce.last = acce.inst.getOrientation();
    }
    setRotation();
  });

  // This will turn the back light on (full brightness)
  // #define LCD_BACK_LIGHT_PIN 21
  //   digitalWrite(LCD_BACK_LIGHT_PIN, HIGH);

  // Setup the WiFi
  WiFi.setHostname(Settings.AP.SSID.c_str());
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  // WiFi connected
  WiFi.onEvent(
      [](WiFiEvent_t event, WiFiEventInfo_t info) {
        uiHeader->setWiFiStatus(true);
        connectToCS();
      },
      ARDUINO_EVENT_WIFI_STA_GOT_IP);
  // WiFi disconnected
  WiFi.onEvent([](WiFiEvent_t event,
                  WiFiEventInfo_t info) { uiHeader->setWiFiStatus(false); },
               ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  // CS connected
  csClient.onConnect([](void *arg, AsyncClient *client) {
    uiHeader->setCSStatus(true);
    dccExCS.getCSPower(); // Get current power status
  });
  // CS disconnected
  csClient.onDisconnect([](void *arg, AsyncClient *client) {
    uiHeader->setCSStatus(false);
    connectToCS();
  });
  // If the connection to the CS times out then close it so it'll attempt a
  // reconnect
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
        // TODO, only add to buffer if we're in a command?
        csBuffer[csBufferLen++] = static_cast<uint8_t *>(data)[i];
      }
    }
  });

  // Aquired loco count change
  locos.addEventListener(Locos::Event::COUNT_CHANGE, [](void *parameter) {
    auto count = *static_cast<uint8_t *>(parameter);
    uiHeader->setLocoCount(count);
  });

  // CS Power event
  dccExCS.addEventListener(DCCExCS::Event::BROADCAST_POWER,
                           [](void *parameter) {
                             power = *static_cast<DCCExCS::Power *>(
                                 parameter); // Remember current power states
                           });

  // If the CS settings change we disconnect so the keep alive task will
  // reconnect using new values
  Settings.addEventListener(SettingsClass::Event::CS_CHANGE,
                            [](void *) { WiFi.disconnect(); });

  xTaskCreatePinnedToCore(powerCheck, "powerCheck", 1024, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(keepWiFiAlive, "keepWiFiAlive", 2048, NULL, 1, NULL,
                          1);

  // Create UI Header
  uiHeader = std::make_unique<UIHeader>();
  uiHeader->addEventListener(UIHeader::Event::MENU, [](void *) {
    if (bottomBar != nullptr) {
      switch (bottomBar->getActiveTab()) {
      case BottomBar::Tab::LOCO:
        setLocoUI();
        break;
      case BottomBar::Tab::ACCESSORIES:
        setAccessoriesUI();
        break;
      case BottomBar::Tab::POWER:
        setPowerUI();
        break;
      case BottomBar::Tab::SETTINGS:
        setSettingsUI();
        break;
      }
    }
  });

  // Create Bottom Bar UI
  bottomBar = std::make_unique<BottomBar>();
  bottomBar->addEventListener(BottomBar::Event::NAVIGATE, [](void *parameter) {
    auto tab = *static_cast<BottomBar::Tab *>(parameter);
    switch (tab) {
    case BottomBar::Tab::LOCO:
      setLocoUI();
      break;
    case BottomBar::Tab::ACCESSORIES:
      setAccessoriesUI();
      break;
    case BottomBar::Tab::POWER:
      setPowerUI();
      break;
    case BottomBar::Tab::SETTINGS:
      setSettingsUI();
      break;
    }
  });

  // Start with the Loco tab (shows Selector if locos == 0)
  setLocoUI();
}

void loop() {
  if (setUI != nullptr) {
    UI::tft->fillRect(0, (30 * TFT_HEIGHT) / 480, TFT_WIDTH,
                      TFT_HEIGHT - (30 * TFT_HEIGHT) / 480, UI::COLOR_MAIN_BG);
    activeUI.reset();
    activeUI = setUI();
    setUI = nullptr;
    if (activeUI != nullptr) {
      activeUI->handleRedraw();
    }

    if (bottomBar != nullptr) {
      bottomBar->handleRedraw();
    }
  }



  TouchPoint points[5];
  uint8_t touches = getTouchPoints(points);
  if (touches) {
    if (uiHeader == nullptr ||
        !uiHeader->handleTouch(touches, points, isTouched)) {
#define NAV_BAR_Y (((425) * TFT_HEIGHT) / 480)
      if (bottomBar != nullptr && points[0].y >= NAV_BAR_Y) {
        bottomBar->handleTouch(touches, points, isTouched);
      } else if (activeUI != nullptr) {
        activeUI->handleTouch(touches, points, isTouched);
        Serial.println(ESP.getFreeHeap());
      }
    }

  } else if (Settings.rotation == SettingsClass::Rotation::ACCELEROMETER &&
             activeUI != nullptr && acce.enabled) {
    DFRobot_LIS2DW12::eOrient_t orientation = acce.inst.getOrientation();
    if (orientation != acce.last && (orientation == DFRobot_LIS2DW12::eYDown ||
                                     orientation == DFRobot_LIS2DW12::eYUp)) {
      acce.last = orientation;
      setRotation();
    }
  }

  if (uiHeader != nullptr) {
    uiHeader->handleTasks();
  }

  activeUI->handleTasks();
  activeUI->handleLoop();

  delay(1);
}
