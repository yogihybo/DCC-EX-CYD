#ifndef SPEED_DIRECTION_CARD_H
#define SPEED_DIRECTION_CARD_H

#include <Arduino.h>
#include <Element.h>
#include <Events.h>

class SpeedDirectionCard : public Element {
  private:
    uint8_t _speed;
    bool _direction;
  public:
    struct Event {
      static constexpr uint8_t DIRECTION_TOGGLE = 0;
    };

    SpeedDirectionCard(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint8_t speed = 0, bool direction = true);

    inline Type getType() override { return Type::SpeedDirectionCard; }
    void draw() override;
    
    void setSpeed(uint8_t speed, bool redraw = true);
    void setDirection(bool direction, bool redraw = true);

    bool contains(uint16_t x, uint16_t y) override;
};

#endif
