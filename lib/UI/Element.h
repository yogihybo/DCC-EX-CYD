#ifndef ELEMENT_H
#define ELEMENT_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Touch.h>

#if defined(CYD_ESP32)
// Reference UI coordinates are based on 320x480 portrait screen
#define REFERENCE_WIDTH  320
#define REFERENCE_HEIGHT 480

// Scale coordinates to target display width/height
#define SCALE_X(x) (((x) * TFT_WIDTH) / REFERENCE_WIDTH)
#define SCALE_Y(y) (((y) * TFT_HEIGHT) / REFERENCE_HEIGHT)
#else
#define SCALE_X(x) (x)
#define SCALE_Y(y) (y)
#endif

class Element : public Touch {
  protected:
    uint16_t _x, _y;
    uint16_t _w, _h;
    // TODO, move to separate class?
    void fillBorderRoundRect(uint32_t fill, uint32_t border, uint8_t radius = 4, uint8_t width = 2, uint8_t quadrants = 0xF);
  public:
    enum class Type : uint8_t {
      Button,
      Header,
      Image,
      Input,
      Label,
      TabButton,
      LocoAddressCard,
      SpeedDirectionCard
    };

    Element(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    virtual ~Element() = default;
    virtual Type getType() = 0;
    virtual void draw() = 0;
};

#endif
