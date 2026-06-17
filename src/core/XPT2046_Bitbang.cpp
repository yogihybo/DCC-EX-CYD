/* 
 * Based on XPT2046_Bitbang_Arduino_Library by ddxfish (https://github.com/ddxfish/XPT2046_Bitbang_Arduino_Library)
 * Modified to integrate with application Settings and remove SPIFFS dependency.
 */

#include "XPT2046_Bitbang.h"
#include <Settings.h>

#define CMD_READ_Y    0x90 // Command for XPT2046 to read Y position
#define CMD_READ_X    0xD0 // Command for XPT2046 to read X position
#define CMD_READ_VBAT 0xA6 // Command for XPT2046 to read VBAT (pin 7); internal ÷4 divider, 2.5V ref

XPT2046_Bitbang::XPT2046_Bitbang(uint8_t mosiPin, uint8_t misoPin, uint8_t clkPin, uint8_t csPin, uint8_t irqPin) : 
    _mosiPin(mosiPin), _misoPin(misoPin), _clkPin(clkPin), _csPin(csPin), _irqPin(irqPin) {
}

void XPT2046_Bitbang::begin() {
    pinMode(_mosiPin, OUTPUT);
    pinMode(_misoPin, INPUT);
    pinMode(_clkPin, OUTPUT);
    pinMode(_csPin, OUTPUT);
    pinMode(_irqPin, INPUT); 
    digitalWrite(_csPin, HIGH);
    digitalWrite(_clkPin, LOW);
}

int XPT2046_Bitbang::readSPI(byte command) {
    int result = 0;

    for (int i = 7; i >= 0; i--) {
        digitalWrite(_mosiPin, command & (1 << i));
        digitalWrite(_clkPin, HIGH);
        delayMicroseconds(2);
        digitalWrite(_clkPin, LOW);
        delayMicroseconds(2);
    }

    // Dummy clock for BUSY bit
    digitalWrite(_clkPin, HIGH);
    delayMicroseconds(2);
    digitalWrite(_clkPin, LOW);
    delayMicroseconds(2);

    for (int i = 11; i >= 0; i--) {
        digitalWrite(_clkPin, HIGH);
        delayMicroseconds(2);
        result |= (digitalRead(_misoPin) << i);
        digitalWrite(_clkPin, LOW);
        delayMicroseconds(2);
    }

    return result;
}

bool XPT2046_Bitbang::isTouched() {
    // IRQ pin goes LOW when touched
    return digitalRead(_irqPin) == LOW;
}

static int16_t besttwoavg(int16_t a, int16_t b, int16_t c) {
  int16_t dab = abs(a - b);
  int16_t dbc = abs(b - c);
  int16_t dca = abs(c - a);
  if (dab <= dbc && dab <= dca ) return (a + b) / 2;
  else if (dbc <= dab && dbc <= dca) return (b + c) / 2;
  else return (a + c) / 2;
}

float XPT2046_Bitbang::readBattery() {
    digitalWrite(_csPin, LOW);
    int raw = readSPI(CMD_READ_VBAT);
    digitalWrite(_csPin, HIGH);
    // XPT2046 VBAT channel: Vref = 2.5V internal, internal ÷4 attenuator, 12-bit ADC
    return (raw / 4096.0f) * 2.5f * 4.0f;
}

Point XPT2046_Bitbang::getTouch() {
    // Helper lambda to perform a single isolated transaction
    auto getSample = [this]() -> Point {
        digitalWrite(_csPin, LOW);
        int x = readSPI(CMD_READ_X);
        int y = readSPI(CMD_READ_Y);
        digitalWrite(_csPin, HIGH);
        return Point{x, y};
    };

    // Dummy read to wake up the chip and settle the ADC
    getSample();

    // Take 3 completely independent samples
    Point p1 = getSample();
    Point p2 = getSample();
    Point p3 = getSample();

    // Apply median filter
    int filteredX = besttwoavg(p1.x, p2.x, p3.x);
    int filteredY = besttwoavg(p1.y, p2.y, p3.y);
    
    return Point{filteredX, filteredY};
}
