#include <UIHeader.h>

UIHeader::UIHeader() {
  _power = addElement<Image>(0, (2 * TFT_HEIGHT) / 480, (60 * TFT_WIDTH) / 320, (30 * TFT_HEIGHT) / 480, SPIFFS);
  _wifi = addElement<Image>((70 * TFT_WIDTH) / 320, 0, (30 * TFT_WIDTH) / 320, (30 * TFT_HEIGHT) / 480, SPIFFS);
  _cs = addElement<Image>((110 * TFT_WIDTH) / 320, 0, (30 * TFT_WIDTH) / 320, (30 * TFT_HEIGHT) / 480, SPIFFS);

  addElement<Image>((150 * TFT_WIDTH) / 320, 0, (30 * TFT_WIDTH) / 320, (30 * TFT_HEIGHT) / 480, "/header/conductor.bmp", SPIFFS);
  _locos = addElement<Label>((180 * TFT_WIDTH) / 320, (10 * TFT_HEIGHT) / 480, (100 * TFT_WIDTH) / 320, (18 * TFT_HEIGHT) / 480);

  updatePowerStatus();
  updateWiFiStatus();
  updateCSStatus();
  updateLocoCount();

  addElement<Button>((290 * TFT_WIDTH) / 320, 0, (30 * TFT_WIDTH) / 320, (30 * TFT_HEIGHT) / 480, Button::Appearance {
    "/header/menu.bmp",
    SPIFFS
  })->onRelease([this](void*) {
    dispatchEvent(Event::MENU);
  });
}

void UIHeader::updatePowerStatus() {
  if (_powerStatus == PowerStatus::STATUS_CHARGING) { // If we can detect charging in the future
    _power->setImage("/header/battery-charging.bmp");
  } else if (_powerStatus == PowerStatus::STATUS_HIGH) {
    _power->setImage("/header/battery-full.bmp");
  } else if (_powerStatus == PowerStatus::STATUS_MEDIUM) {
    _power->setImage("/header/battery-medium.bmp");
  } else if (_powerStatus == PowerStatus::STATUS_LOW) {
    _power->setImage("/header/battery-low.bmp");
  } else if (_powerStatus == PowerStatus::STATUS_RECHARGE) {
    _power->setImage("/header/battery-recharge.bmp");
  }
}

void UIHeader::setPowerStatus(float voltage) {
  if (voltage > 4.0 && _powerStatus != PowerStatus::STATUS_HIGH) {
    _powerStatus = PowerStatus::STATUS_HIGH;
  } else if (voltage > 3.85 && voltage <= 3.99 && _powerStatus != PowerStatus::STATUS_MEDIUM) {
    _powerStatus = PowerStatus::STATUS_MEDIUM;
  } else if (voltage > 3.75 && voltage <= 3.84 && _powerStatus != PowerStatus::STATUS_LOW) {
    _powerStatus = PowerStatus::STATUS_LOW;
  } else if (voltage < 3.74 && _powerStatus != PowerStatus::STATUS_RECHARGE) {
    _powerStatus = PowerStatus::STATUS_RECHARGE;
  }

  _tasks.push_back([this] {
    updatePowerStatus();
  });
}

void UIHeader::updateWiFiStatus() {
  _wifi->setImage(_wifiStatus
    ? "/header/wifi-connected.bmp"
    : "/header/wifi-disconnected.bmp");
}

void UIHeader::setWiFiStatus(bool connected) {
  if (_wifiStatus != connected) {
    _wifiStatus = connected;
    _tasks.push_back([this] {
      updateWiFiStatus();
    });
  }
}

void UIHeader::updateCSStatus() {
  _cs->setImage(_csStatus
    ? "/header/cs-connected.bmp"
    : "/header/cs-disconnected.bmp");
}

void UIHeader::setCSStatus(bool connected) {
  if (_csStatus != connected) {
    _csStatus = connected;
    _tasks.push_back([this] {
      updateCSStatus();
    });
  }
}

void UIHeader::updateLocoCount() {
  String label(": ");
  label += _locoCount;
  _locos->setLabel(label);
}

void UIHeader::setLocoCount(uint8_t count) {
  if (_locoCount != count) {
    _locoCount = count;
    updateLocoCount();
  }
}
