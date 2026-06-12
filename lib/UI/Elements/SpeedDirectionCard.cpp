#include <Elements/SpeedDirectionCard.h>
#include <UI.h>

SpeedDirectionCard::SpeedDirectionCard(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                       uint8_t speed, bool direction)
    : Element(x, y, w, h), _speed(speed), _direction(direction) {
  onRelease([this](void*) {
    dispatchEvent(Event::DIRECTION_TOGGLE, nullptr);
  });
}

void SpeedDirectionCard::draw() {
  // 1. Draw card container
  fillBorderRoundRect(UI::COLOR_CARD_BG, UI::COLOR_BORDER, 6, 1, 0xF);

  uint16_t cx = _x + _w / 2;
  uint16_t cy = _y + 60; // Center of the speed gauge

  uint32_t oldTextColor = UI::tft->textcolor;
  uint32_t oldTextBGColor = UI::tft->textbgcolor;
  uint8_t oldDatum = UI::tft->getTextDatum();

  // 2. Draw Title "SPEED & DIRECTION"
  UI::tft->setTextDatum(MC_DATUM);
  UI::tft->setTextColor(UI::COLOR_TEXT_MUTED, UI::COLOR_CARD_BG);
  UI::tft->drawString("SPEED & DIRECTION", cx, _y + 15);

  // 3. Draw Speed Gauge
  // Draw base track (inactive arc from 220 to 500 degrees)
  UI::tft->drawSmoothArc(cx, cy, 32, 27, 220, 500, UI::COLOR_BORDER, UI::COLOR_CARD_BG, false);

  // Draw active arc if speed > 0
  if (_speed > 0) {
    uint32_t activeSpan = (_speed * 280) / 126;
    if (activeSpan > 280) activeSpan = 280;
    UI::tft->drawSmoothArc(cx, cy, 32, 27, 220, 220 + activeSpan, UI::COLOR_CYAN, UI::COLOR_CARD_BG, false);
  }

  // 4. Draw Speed Value inside the gauge
  UI::tft->setTextColor(TFT_WHITE, UI::COLOR_CARD_BG);
  UI::tft->drawString(String(_speed), cx, cy - 4);
  
  // Draw "KPH" underneath the speed
  UI::tft->setTextColor(UI::COLOR_TEXT_MUTED, UI::COLOR_CARD_BG);
  UI::tft->drawString("KPH", cx, cy + 14);

  // 5. Draw Direction Capsule
  uint16_t capY = _y + 104;
  uint16_t capColor = _direction ? UI::COLOR_GREEN : UI::COLOR_BORDER;
  uint16_t textColor = _direction ? TFT_BLACK : TFT_WHITE;
  String text = _direction ? "FWD [>>]" : "REV [<<]";

  // Fill capsule background
  UI::tft->fillSmoothRoundRect(cx - 45, capY, 90, 24, 5, capColor, UI::COLOR_CARD_BG);
  
  // Draw capsule text
  UI::tft->setTextColor(textColor, capColor);
  UI::tft->drawString(text, cx, capY + 12);

  UI::tft->setTextDatum(oldDatum);
  UI::tft->setTextColor(oldTextColor, oldTextBGColor);
}

void SpeedDirectionCard::setSpeed(uint8_t speed, bool redraw) {
  if (_speed != speed) {
    _speed = speed;
    if (redraw) {
      draw();
    }
  }
}

void SpeedDirectionCard::setDirection(bool direction, bool redraw) {
  if (_direction != direction) {
    _direction = direction;
    if (redraw) {
      draw();
    }
  }
}

bool SpeedDirectionCard::contains(uint16_t x, uint16_t y) {
  uint16_t cx = _x + _w / 2;
  uint16_t capX = cx - 45;
  uint16_t capY = _y + 104;
  uint16_t capW = 90;
  uint16_t capH = 24;

  // Check if the touch is within the direction capsule bounds
  return (x >= capX && x <= capX + capW &&
          y >= capY && y <= capY + capH);
}
