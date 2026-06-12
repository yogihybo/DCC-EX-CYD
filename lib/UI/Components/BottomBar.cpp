#include <Components/BottomBar.h>

BottomBar::BottomBar() : _activeTab(Tab::LOCO) {
  _elements.reserve(4);

  // Bottom bar is located at y = 425 to 480 (height 55)
  // Width is 320. 4 tabs. Width of each is 80.
  uint16_t w = 80;
  uint16_t h = 55;
  uint16_t y = 425;

  _buttons[0] = addElement<TabButton>(0, y, w, h, TabButton::Icon::LOCO, true);
  _buttons[0]->onRelease([this](void*) { selectTab(Tab::LOCO); });

  _buttons[1] = addElement<TabButton>(80, y, w, h, TabButton::Icon::ACCESSORIES, false);
  _buttons[1]->onRelease([this](void*) { selectTab(Tab::ACCESSORIES); });

  _buttons[2] = addElement<TabButton>(160, y, w, h, TabButton::Icon::POWER, false);
  _buttons[2]->onRelease([this](void*) { selectTab(Tab::POWER); });

  _buttons[3] = addElement<TabButton>(240, y, w, h, TabButton::Icon::SETTINGS, false);
  _buttons[3]->onRelease([this](void*) { selectTab(Tab::SETTINGS); });
}

void BottomBar::selectTab(Tab tab, bool fireEvent) {
  if (_activeTab != tab) {
    _activeTab = tab;
    for (uint8_t i = 0; i < 4; i++) {
      _buttons[i]->setActive(static_cast<Tab>(i) == _activeTab, true);
    }
    if (fireEvent) {
      dispatchEvent(Event::NAVIGATE, &_activeTab);
    }
  }
}

BottomBar::Tab BottomBar::getActiveTab() const {
  return _activeTab;
}

void BottomBar::setActiveTab(Tab tab) {
  selectTab(tab, false);
}

void BottomBar::redraw() {
  // Draw top border line separating bottom bar from central content
  UI::tft->drawFastHLine(0, SCALE_Y(424), TFT_WIDTH, TFT_DARKGREY);
}
