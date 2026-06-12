#include <LocoUI.h>
#include <SPIFFS.h>
#include <SD.h>
#include <StreamUtils.h>
#include <Settings.h>
#include <Components/Paging.h>
#include <Elements/Header.h>
#include <Elements/Button.h>
#include <Functions.h>

LocoUI::LocoUI(DCCExCS& dccExCS, uint16_t address) : _dccExCS(dccExCS), _loco(address), _broadcastLocoHandler(0xFFFF) {
  _elements.reserve(37);

  if (address == 0) {
    addElement<Header>(0, 40, 320, 18, "Select Locomotive");

    addElement<Button>(10, 90, 300, 60, "By Address")
      ->onRelease([this](void*) {
        uint8_t action = SettingsClass::LocoUI::Swipe::Action::KEYPAD;
        dispatchEvent(Event::SWIPE_ACTION, &action);
      });

    addElement<Button>(10, 170, 300, 60, "By Name")
      ->onRelease([this](void*) {
        uint8_t action = SettingsClass::LocoUI::Swipe::Action::NAME;
        dispatchEvent(Event::SWIPE_ACTION, &action);
      });

    addElement<Button>(10, 250, 300, 60, "By Group")
      ->onRelease([this](void*) {
        uint8_t action = SettingsClass::LocoUI::Swipe::Action::GROUP;
        dispatchEvent(Event::SWIPE_ACTION, &action);
      });
    return;
  }

  _broadcastLocoHandler = _dccExCS.addEventListener(DCCExCS::Event::BROADCAST_LOCO, [this](void* parameter) {
    broadcast(parameter);
  });

  char path[32];
  sprintf(path, "/locos/%d.json", _loco.address);
  
  if (SPIFFS.exists(path)) {
    File locoFile = SPIFFS.open(path);
    ReadBufferingStream bufferedFile(locoFile, _locoDoc.capacity());
    deserializeJson(_locoDoc, bufferedFile);
    locoFile.close();
  } else if (SD.exists(path)) { // Check for loco config file
    File locoFile = SD.open(path);
    ReadBufferingStream bufferedFile(locoFile, _locoDoc.capacity());
    deserializeJson(_locoDoc, bufferedFile);
    locoFile.close();
  }

  // Max 20 chars for name
  String nameStr = "";
  if (_locoDoc.containsKey("name")) {
    nameStr = _locoDoc["name"].as<const char*>();
  }

  // Create Loco Address Card (x = 10, y = 40, w = 145, h = 140)
  _addressCard = addElement<LocoAddressCard>(10, 40, 145, 140, _loco.address, nameStr);

  // Create Speed & Direction Card (x = 165, y = 40, w = 145, h = 140)
  _speedCard = addElement<SpeedDirectionCard>(165, 40, 145, 140, _loco.speed, _loco.direction);
  _speedCard->addEventListener(SpeedDirectionCard::Event::DIRECTION_TOGGLE, [this](void*) {
    _dccExCS.setLocoThrottle(_loco.address, _loco.speed, !_loco.direction);
  });

  if (_locoDoc["functions"].is<JsonArray>()) { // Function map array part of loco json
    _locoFunctions = _locoDoc["functions"].as<JsonArray>();
  } else {
    File functionFile;
    if (_locoDoc.containsKey("functions")) {
      const char* fnPath = _locoDoc["functions"].as<const char*>();
      if (SPIFFS.exists(fnPath)) {
        functionFile = SPIFFS.open(fnPath);
      } else if (SD.exists(fnPath)) {
        functionFile = SD.open(fnPath);
      }
    }
    if (!functionFile) { // Name of function map file or default
      functionFile = SPIFFS.open("/default.json");
    }

    StaticJsonDocument<16> filter;
    filter["functions"] = true;

    ReadBufferingStream bufferedFile(functionFile, _locoDoc.capacity());
    deserializeJson(_locoDoc, functionFile, DeserializationOption::Filter(filter));
    _locoFunctions = _locoDoc["functions"].as<JsonArray>();
    functionFile.close();
  }

  uint8_t rows = _locoFunctions.size();

  if (rows > 4) { // More than 4 rows and we need paging
    uint8_t pages = divideAndCeil(rows, 3);
    auto paging = addComponent<Paging>(pages, 0, 365, 320);
    paging->addEventListener(Paging::Event::CHANGED, [this](void* parameter) {
      _page = *static_cast<uint8_t*>(parameter);
      destroyFunctionButtons();
      createFunctionButtons();
    });
  }

  createFunctionButtons();

  _dccExCS.acquireLoco(address);
}

LocoUI::~LocoUI() {
  if (_broadcastLocoHandler != 0xFFFF) {
    _dccExCS.removeEventListener(DCCExCS::Event::BROADCAST_LOCO, _broadcastLocoHandler);
  }
}

void LocoUI::broadcast(void* parameter) {
  auto broadcast = *static_cast<DCCExCS::Loco*>(parameter);
  if (_loco.address == broadcast.address) { // Broadcast for loco we're currently controlling
    _tasks.push_back([this, loco = _loco, broadcast] {
      if (loco.speed != broadcast.speed) {
        _speedCard->setSpeed(broadcast.speed);
      }
      if (loco.direction != broadcast.direction) {
        _speedCard->setDirection(broadcast.direction);
      }
      if (loco.functions != broadcast.functions) {
        toggleFunctionButtons(loco.functions ^ broadcast.functions);
      }
    });
    _loco = broadcast;
    _speedPending = false;
  }
}

void LocoUI::createFunctionButtons() {
  bool paging = _locoFunctions.size() > 4;
  
  // Draw functions card background
  UI::tft->fillSmoothRoundRect(10, 195, 300, 220, 6, UI::COLOR_CARD_BG, UI::COLOR_MAIN_BG);
  UI::tft->drawSmoothRoundRect(10, 195, 6, 1, 300, 220, UI::COLOR_BORDER, UI::COLOR_MAIN_BG);

  uint8_t oldDatum = UI::tft->getTextDatum();
  uint32_t oldTextColor = UI::tft->textcolor;
  uint32_t oldTextBGColor = UI::tft->textbgcolor;

  UI::tft->setTextDatum(MC_DATUM);
  UI::tft->setTextColor(UI::COLOR_TEXT_MUTED, UI::COLOR_CARD_BG);
  UI::tft->drawString("FUNCTIONS", 160, 210);

  UI::tft->setTextDatum(oldDatum);
  UI::tft->setTextColor(oldTextColor, oldTextBGColor);
  
  uint8_t i = 0;
  uint16_t y = 225;

  for (JsonArrayConst const& row : _locoFunctions) {
    if (!paging || divideAndCeil(++i, 3) == _page) {
      uint8_t cols = row.size();
      uint16_t totalW = 280;
      uint16_t width = (totalW - ((cols - 1) * 7)) / cols;

      uint16_t x = 20;
      uint8_t col = 0;
      for (JsonObjectConst const& fn : row) {
        uint8_t extra = cols == 4 && (col == 1 || col == 2) ? 1 : 0;
        bool latching = fn["latching"] | true;
        uint8_t func = fn["fn"];

        auto appearance = [](JsonObjectConst obj, bool active) -> Button::Appearance {
          const char* label = obj["label"].as<const char*>();
          const char* icon = obj["icon"].as<const char*>();
          
          uint16_t textCol = active ? TFT_BLACK : TFT_WHITE;
          uint16_t fillCol = active ? UI::COLOR_CYAN : UI::COLOR_CARD_BG;
          uint16_t borderCol = active ? UI::COLOR_CYAN : UI::COLOR_BORDER;

          if (strncmp(icon, "/$/", 3) == 0) {
            return {
              label,
              textCol,
              fillCol,
              Button::Appearance::Border(borderCol, 4),
              icon + 2,
              SPIFFS
            };
          }

          return {
            label,
            textCol,
            fillCol,
            Button::Appearance::Border(borderCol, 4),
            icon
          };
        };

        bool isActive = _loco.functions.test(func);
        auto btn = addElement<Button>(x, y, width + extra, 36,
          appearance(fn["btn"]["idle"], false),
          appearance(fn["btn"]["pressed"], true),
          latching,
          isActive ? Button::State::PRESSED : Button::State::IDLE
        );
        btn->setBg(UI::COLOR_CARD_BG, true);
        btn->onTouch([this, latching, func](void*) {
          if (latching) {
            _dccExCS.setLocoFn(_loco.address, func, _loco.functions.flip(func).test(func));
          } else {
            _dccExCS.setLocoFn(_loco.address, func, true);
          }
        });
        btn->onRelease([this, latching, func](void*) {
          if (!latching) {
            _dccExCS.setLocoFn(_loco.address, func, false);
          }
        });

        x += width + 7 + extra;
        col++;
      }
      y += 43;
    }
  }
}

void LocoUI::destroyFunctionButtons() {
  _elements.erase(std::remove_if(_elements.begin(), _elements.end(), [](const auto &element) {
    return element->getType() == Element::Type::Button;
  }), _elements.end());
}

void LocoUI::toggleFunctionButtons(std::bitset<32> toggle) {
  uint8_t i = 0;
  auto btnFirst = std::find_if(_elements.begin(), _elements.end(), [](const auto &element) {
    return element->getType() == Element::Type::Button;
  });
  uint8_t btnIndex = std::distance(_elements.begin(), btnFirst);
  uint8_t btnCount = _elements.size();
  bool paging = _locoFunctions.size() > 4;

  for (JsonArrayConst const& row : _locoFunctions) {
    if (!paging || divideAndCeil(++i, 3) == _page) {
      for (JsonObjectConst const& fn : row) {
        uint8_t func = fn["fn"];
        if (toggle.test(func) && btnIndex < btnCount) {
          static_cast<Button*>(_elements[btnIndex].get())
            ->setState(_loco.functions.test(func) ? Button::State::PRESSED : Button::State::IDLE, true);
        }
        btnIndex++;
      }
    }
  }
}

bool LocoUI::encoderRotate(Encoder::Rotation rotation) {
  if (_speedPending) {
    return true;
  }

  int8_t step = rotation == Encoder::Rotation::CW ? 1 : -1;
  switch (Settings.LocoUI.speedStep) {
    case SettingsClass::LocoUI::SpeedStep::STEP_2: {
      step *= 2;
    } break;
    case SettingsClass::LocoUI::SpeedStep::STEP_4: {
      step *= 4;
    } break;
  }
  
  int8_t speed = max<int16_t>(min<int16_t>(_loco.speed + step, 126), 0);
  if (speed != _loco.speed) {
    _speedPending = true;
    _dccExCS.setLocoThrottle(_loco.address, speed, _loco.direction);
  }
  
  return true;
}

bool LocoUI::encoderPress(Encoder::ButtonPress press) {
  if (press == Encoder::ButtonPress::SHORT) { // Change loco direction
    _dccExCS.setLocoThrottle(_loco.address, _loco.speed, !_loco.direction);
  }

  return true;
}

bool LocoUI::swipe(Swipe swipe) {
  switch (swipe) {
    case Swipe::UP: {
      dispatchEvent(Event::SWIPE_ACTION, &Settings.LocoUI.Swipe.up);
    } break;
    case Swipe::DOWN: {
      dispatchEvent(Event::SWIPE_ACTION, &Settings.LocoUI.Swipe.down);
    } break;
    case Swipe::LEFT: {
      dispatchEvent(Event::SWIPE_ACTION, &Settings.LocoUI.Swipe.left);
    } break;
    case Swipe::RIGHT: {
      dispatchEvent(Event::SWIPE_ACTION, &Settings.LocoUI.Swipe.right);
    } break;
  }

  return true;
}
