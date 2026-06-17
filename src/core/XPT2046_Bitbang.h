/* 
 * Based on XPT2046_Bitbang_Arduino_Library by ddxfish (https://github.com/ddxfish/XPT2046_Bitbang_Arduino_Library)
 * Modified to integrate with application Settings and remove SPIFFS dependency.
 */

#ifndef XPT2046_Bitbang_h
#define XPT2046_Bitbang_h

#include "Arduino.h"

#ifndef TFT_WIDTH
#define TFT_WIDTH 240
#endif

#ifndef TFT_HEIGHT
#define TFT_HEIGHT 320
#endif

struct Point {
    int x;
    int y;
};

class XPT2046_Bitbang {
public:
    XPT2046_Bitbang(uint8_t mosiPin, uint8_t misoPin, uint8_t clkPin, uint8_t csPin, uint8_t irqPin);
    void begin();
    Point getTouch();
    bool isTouched();
    float readBattery(); // Returns battery voltage in volts via XPT2046 VBAT channel (pin 7)

private:
    uint8_t _mosiPin;
    uint8_t _misoPin;
    uint8_t _clkPin;
    uint8_t _csPin;
    uint8_t _irqPin;
    int readSPI(byte command);
};

#endif
