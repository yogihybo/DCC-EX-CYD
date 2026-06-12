#include <Elements/TabButton.h>
#include <UI.h>

TabButton::TabButton(int16_t x, int16_t y, uint16_t w, uint16_t h, Icon icon, bool active)
    : Element(x, y, w, h), _icon(icon), _active(active) {
  draw();
  onTouch([this](void*) {
    // Tab changes are handled by the parent component onRelease
  });
}

void TabButton::setActive(bool active, bool redraw) {
  if (_active != active) {
    _active = active;
    if (redraw) {
      draw();
    }
  }
}

bool TabButton::isActive() const {
  return _active;
}

TabButton::Icon TabButton::getIcon() const {
  return _icon;
}

void TabButton::draw() {
  uint16_t color = _active ? UI::COLOR_CYAN : UI::COLOR_TEXT_MUTED;
  
  // Clear the tab button background
  UI::tft->fillRect(_x, _y, _w, _h, UI::COLOR_MAIN_BG);
  
  uint16_t cx = _x + _w / 2;
  uint16_t cy = _y + _h / 2;

  // Draw active tab indicator capsule
  if (_active) {
    UI::tft->fillSmoothRoundRect(cx - 20, cy - 12, 40, 24, 6, UI::COLOR_CYAN_BG, UI::COLOR_MAIN_BG);
  }

  switch (_icon) {
    case Icon::LOCO: {
      // Draw Locomotive Cab outline
      UI::tft->drawRect(cx - 8, cy - 4, 16, 10, color); // Cab body
      UI::tft->drawRect(cx + 4, cy, 6, 6, color);      // Cab nose
      UI::tft->drawLine(cx - 9, cy - 6, cx + 4, cy - 6, color); // Roof line
      UI::tft->drawRect(cx - 3, cy - 2, 4, 4, color);  // Window
      UI::tft->drawCircle(cx - 5, cy + 9, 2, color);   // Wheel 1
      UI::tft->drawCircle(cx, cy + 9, 2, color);      // Wheel 2
      UI::tft->drawCircle(cx + 5, cy + 9, 2, color);   // Wheel 3
      break;
    }
    case Icon::ACCESSORIES: {
      // Draw Turnout / Switch symbol
      UI::tft->drawLine(cx - 10, cy + 4, cx + 10, cy + 4, color); // Main track
      UI::tft->drawLine(cx - 2, cy + 4, cx + 8, cy - 6, color);   // Diverging track
      
      // Sleepers (vertical ticks)
      UI::tft->drawLine(cx - 8, cy + 2, cx - 8, cy + 6, color);
      UI::tft->drawLine(cx - 4, cy + 2, cx - 4, cy + 6, color);
      UI::tft->drawLine(cx, cy + 2, cx, cy + 6, color);
      UI::tft->drawLine(cx + 4, cy + 2, cx + 4, cy + 6, color);
      UI::tft->drawLine(cx + 8, cy + 2, cx + 8, cy + 6, color);
      break;
    }
    case Icon::POWER: {
      // Draw Lightning Bolt
      UI::tft->fillTriangle(cx + 3, cy - 9, cx - 5, cy + 1, cx + 1, cy + 1, color); // Top half
      UI::tft->fillTriangle(cx - 1, cy + 1, cx - 3, cy + 9, cx + 5, cy - 1, color); // Bottom half
      break;
    }
    case Icon::SETTINGS: {
      // Draw Gear outline
      UI::tft->drawCircle(cx, cy, 7, color);
      UI::tft->drawCircle(cx, cy, 2, color);
      
      // Teeth
      UI::tft->drawLine(cx - 1, cy - 9, cx + 1, cy - 9, color); // Up
      UI::tft->drawLine(cx - 1, cy + 9, cx + 1, cy + 9, color); // Down
      UI::tft->drawLine(cx - 9, cy - 1, cx - 9, cy + 1, color); // Left
      UI::tft->drawLine(cx + 9, cy - 1, cx + 9, cy + 1, color); // Right
      
      // Diagonals
      UI::tft->drawLine(cx - 6, cy - 6, cx - 7, cy - 7, color);
      UI::tft->drawLine(cx + 6, cy - 6, cx + 7, cy - 7, color);
      UI::tft->drawLine(cx - 6, cy + 6, cx - 7, cy + 7, color);
      UI::tft->drawLine(cx + 6, cy + 6, cx + 7, cy + 7, color);
      break;
    }
  }
}
