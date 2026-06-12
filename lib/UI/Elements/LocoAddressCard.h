#ifndef LOCO_ADDRESS_CARD_H
#define LOCO_ADDRESS_CARD_H

#include <Arduino.h>
#include <Element.h>

class LocoAddressCard : public Element {
  private:
    uint16_t _address;
    String _name;
  public:
    LocoAddressCard(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                    uint16_t address, const String& name = "");

    inline Type getType() override { return Type::LocoAddressCard; }
    void draw() override;
    void setAddress(uint16_t address, const String& name = "", bool redraw = true);
};

#endif
