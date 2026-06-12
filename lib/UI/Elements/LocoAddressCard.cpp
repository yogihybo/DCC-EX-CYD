#include <Elements/LocoAddressCard.h>
#include <UI.h>

LocoAddressCard::LocoAddressCard(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                 uint16_t address, const String& name)
    : Element(x, y, w, h), _address(address), _name(name) {
  draw();
}

void LocoAddressCard::draw() {
  // Draw the card container with rounded corners and border
  fillBorderRoundRect(UI::COLOR_CARD_BG, UI::COLOR_BORDER, 6, 1, 0xF);

  uint16_t cx = _x + _w / 2;
  uint16_t cy = _y + _h / 2;

  // 1. Draw Title "LOCO ADDRESS"
  uint32_t oldTextColor = UI::tft->textcolor;
  uint32_t oldTextBGColor = UI::tft->textbgcolor;
  uint8_t oldDatum = UI::tft->getTextDatum();

  UI::tft->setTextDatum(MC_DATUM);
  UI::tft->setTextColor(UI::COLOR_TEXT_MUTED, UI::COLOR_CARD_BG);
  
  // Set smaller text size for title
  UI::tft->drawString("LOCO ADDRESS", cx, _y + 15);

  // 2. Draw Address (Large white text)
  UI::tft->setTextColor(TFT_WHITE, UI::COLOR_CARD_BG);
  String addrText = "# " + String(_address);
  UI::tft->drawString(addrText, cx, _y + 45);

  // 3. Draw Name (if set) in smaller, muted text
  if (_name.length() > 0) {
    UI::tft->setTextColor(UI::COLOR_TEXT_MUTED, UI::COLOR_CARD_BG);
    UI::tft->drawString(_name, cx, _y + 68);
  }

  // 4. Draw Steam Locomotive vector icon
  uint16_t iconColor = UI::COLOR_CYAN;
  int16_t iconY = _y + 90;

  // Boiler
  UI::tft->drawRect(cx - 25, iconY + 10, 36, 18, iconColor);
  // Cab
  UI::tft->drawRect(cx + 11, iconY + 2, 16, 26, iconColor);
  // Cab roof
  UI::tft->drawFastHLine(cx + 8, iconY, 21, iconColor);
  // Cab window
  UI::tft->drawRect(cx + 15, iconY + 6, 8, 8, iconColor);
  // Funnel
  UI::tft->drawRect(cx - 20, iconY, 6, 10, iconColor);
  // Cowcatcher
  UI::tft->drawLine(cx - 25, iconY + 28, cx - 32, iconY + 28, iconColor);
  UI::tft->drawLine(cx - 32, iconY + 28, cx - 25, iconY + 20, iconColor);
  // Wheels
  UI::tft->drawCircle(cx - 15, iconY + 33, 5, iconColor);
  UI::tft->drawCircle(cx, iconY + 33, 5, iconColor);
  UI::tft->drawCircle(cx + 15, iconY + 33, 5, iconColor);

  UI::tft->setTextDatum(oldDatum);
  UI::tft->setTextColor(oldTextColor, oldTextBGColor);
}

void LocoAddressCard::setAddress(uint16_t address, const String& name, bool redraw) {
  if (_address != address || _name != name) {
    _address = address;
    _name = name;
    if (redraw) {
      draw();
    }
  }
}
