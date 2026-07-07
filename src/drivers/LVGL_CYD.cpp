// Based on LVGL_CYD v1.2.2 by Rop Gonggrijp <rop@gonggri.jp>
// https://github.com/ropg/LVGL_CYD  (MIT Licence)
// Vendored and modified to support multiple board variants via build-flag
// configuration rather than a single hardcoded board layout.
//
// Board selection is compile-time via platformio.ini build_flags:
//   TFT_WIDTH / TFT_HEIGHT   — display resolution
//   TFT_BL                   — backlight GPIO
//   TOUCH_MOSI/MISO/CLK/CS/IRQ — touch SPI pins
//   ST7796_DRIVER            — selects the 3.5" ST7796 board path
//   (default, no driver define) — selects the original CYD ILI9341/ST7789 path

#include "LVGL_CYD.h"

// Draw buffer: partial-render mode. Larger screens use a smaller fraction to
// cap static RAM — LVGL just flushes in more chunks, visual result identical.
// 2.8" (240x320): 1/10 = 15.4 KB   3.5" (320x480): 1/20 = 15.4 KB
#if (TFT_WIDTH * TFT_HEIGHT) > 200000
#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT / 20 * (LV_COLOR_DEPTH / 8))
#else
#define DRAW_BUF_SIZE (TFT_WIDTH * TFT_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
#endif
static uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// ─── CYD (ILI9341 / ST7789) — original board hardware ───────────────────────
#if !defined(ST7796_DRIVER)

#include <Wire.h>
#define I2C_SDA         33
#define I2C_SCL         32
#define I2C_ADDR_CST820 0x15

#include <SPI.h>
#define Z_THRESHOLD  300
#define SPI_SETTING  SPISettings(2000000, MSBFIRST, SPI_MODE0)
static SPIClass touchSPI(VSPI);

static void touch_read_cyd(lv_indev_t * indev, lv_indev_data_t * data);
static int16_t besttwoavg(int16_t a, int16_t b, int16_t c);

#endif  // !ST7796_DRIVER

// RGB LED — pin defaults match the 2.8" CYD; override via build flags for other boards
#ifndef LED_R
#define LED_R        4
#endif
#ifndef LED_G
#define LED_G        16
#endif
#ifndef LED_B
#define LED_B        17
#endif
#define R_CORRECTION 0.25f
#define G_CORRECTION 1.0f
#define B_CORRECTION 0.4f

// ─── Static members ──────────────────────────────────────────────────────────
bool      LVGL_CYD::capacitive;
bool      LVGL_CYD::resistive;
int16_t   LVGL_CYD::pressure;
TFT_eSPI* LVGL_CYD::tft;


// ─── begin() ─────────────────────────────────────────────────────────────────
void LVGL_CYD::begin(lv_display_rotation_t rotation) {

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // TFT_WIDTH / TFT_HEIGHT come from build flags — correct for each board.
  lv_disp_t* display = lv_tft_espi_create(TFT_WIDTH, TFT_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(display, rotation);
  LVGL_CYD::tft = *(TFT_eSPI**) lv_display_get_driver_data(display);

// ── ST7796 board path ────────────────────────────────────────────────────────
#if defined(ST7796_DRIVER)

  // TFT_eSPI sends the correct ST7796 init sequence from its driver file.
  // No inversion or extra gamma needed.
  Serial.printf("[LVGL] ST7796 %dx%d\n", TFT_WIDTH, TFT_HEIGHT);

  // XPT2046 shares the LCD SPI bus on this board — no separate SPI init here.
  // IO36 is a GPI-only pad (no internal pull resistors); use INPUT only.
  // XPT2046 holds IRQ high when idle so digitalRead gives 1 = touch present.
  pinMode(TOUCH_IRQ, INPUT);
  LVGL_CYD::resistive = digitalRead(TOUCH_IRQ);
  if (LVGL_CYD::resistive) Serial.println("[LVGL] Resistive touch detected");

  if (LVGL_CYD::resistive) {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    // Stub — replaced by main.cpp's XPT2046_Bitbang reader via lv_indev_set_read_cb.
    lv_indev_set_read_cb(indev, [](lv_indev_t*, lv_indev_data_t* d) {
      d->state = LV_INDEV_STATE_RELEASED;
    });
  }

// ── CYD (ILI9341 / ST7789) path ─────────────────────────────────────────────
#else

  // Distinguish ILI9341 from ST7789 at runtime by reading register 0xD3.
  // TFT_eSPI is already initialised by lv_tft_espi_create(), so readcommand8() works.
  uint8_t id3 = LVGL_CYD::tft->readcommand8(0xD3, 2);
  uint8_t id4 = LVGL_CYD::tft->readcommand8(0xD3, 3);
  bool ili9341 = (id3 == 0x93 && id4 == 0x41);
  Serial.printf("[LVGL] %s detected\n", ili9341 ? "ILI9341" : "ST7789");

  if (!ili9341) {
    // ST7789 needs colour inversion and a gamma workaround
    LVGL_CYD::tft->invertDisplay(true);
    LVGL_CYD::tft->writecommand(ILI9341_GAMMASET);
    LVGL_CYD::tft->writedata(2);
    delay(120);
    LVGL_CYD::tft->writecommand(ILI9341_GAMMASET);
    LVGL_CYD::tft->writedata(1);
  }

  // Try capacitive touch first (CST820 on I2C at 0x15)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.beginTransmission(I2C_ADDR_CST820);
  LVGL_CYD::capacitive = Wire.endTransmission() == 0;
  Wire.end();

  if (LVGL_CYD::capacitive) {
    Serial.println("[LVGL] Capacitive touch detected");
  } else {
    // XPT2046 resistive: IRQ line is pulled high when the chip is present
    pinMode(TOUCH_IRQ, INPUT_PULLDOWN);
    LVGL_CYD::resistive = digitalRead(TOUCH_IRQ);
    pinMode(TOUCH_IRQ, INPUT);
    if (LVGL_CYD::resistive) {
      Serial.println("[LVGL] Resistive touch detected");
      pinMode(TOUCH_CS, OUTPUT);
      digitalWrite(TOUCH_CS, HIGH);
      touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    }
  }

  if (LVGL_CYD::capacitive || LVGL_CYD::resistive) {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cyd);
  }

#endif  // ST7796_DRIVER

  LVGL_CYD::backlight(0xff);
}


// ─── backlight() ─────────────────────────────────────────────────────────────
// TFT_BL is set per-board in platformio.ini build_flags, so this works for all
// variants without needing to detect capacitive vs resistive hardware.
void LVGL_CYD::backlight(uint8_t brightness) {
  analogWrite(TFT_BL, brightness);
}


// ─── led() ───────────────────────────────────────────────────────────────────
// RGB LED is CYD hardware only. Safe to call on other boards (no-op if pins
// are unconnected), but the guard prevents stray PWM on unrelated peripherals.
static bool led_used_r = false, led_used_g = false, led_used_b = false;

void LVGL_CYD::led(uint8_t red, uint8_t green, uint8_t blue, bool true_color) {
  if (true_color) {
    red   = 255 - (uint8_t)(red   * R_CORRECTION);
    green = 255 - (uint8_t)(green * G_CORRECTION);
    blue  = 255 - (uint8_t)(blue  * B_CORRECTION);
  } else {
    red   = 255 - red;
    green = 255 - green;
    blue  = 255 - blue;
  }
  if (red   < 255) led_used_r = true;
  if (green < 255) led_used_g = true;
  if (blue  < 255) led_used_b = true;
  if (led_used_r) analogWrite(LED_R, red);
  if (led_used_g) analogWrite(LED_G, green);
  if (led_used_b) analogWrite(LED_B, blue);
}


// ─── CYD touch driver (ILI9341 / ST7789 path only) ───────────────────────────
#if !defined(ST7796_DRIVER)

static void touch_read_cyd(lv_indev_t* indev, lv_indev_data_t* data) {
  int x, y;
  lv_disp_t* display = lv_disp_get_default();

  if (LVGL_CYD::capacitive) {
    uint8_t td[5];
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.beginTransmission(I2C_ADDR_CST820);
    Wire.write(0x02);
    Wire.endTransmission(false);
    Wire.requestFrom(I2C_ADDR_CST820, 5);
    for (int i = 0; i < 5; i++) td[i] = Wire.read();
    Wire.end();
    if (td[0] == 0 || td[0] == 0xFF) { data->state = LV_INDEV_STATE_RELEASED; return; }
    x = ((td[1] & 0x0f) << 8) | td[2];
    y = ((td[3] & 0x0f) << 8) | td[4];

  } else {
    int16_t td[6] = {0};
    touchSPI.beginTransaction(SPI_SETTING);
    digitalWrite(TOUCH_CS, LOW);
    touchSPI.transfer(0xB1);
    LVGL_CYD::pressure  = (touchSPI.transfer16(0xC1) >> 3) + 4095;
    LVGL_CYD::pressure -=  touchSPI.transfer16(0x91) >> 3;
    if (LVGL_CYD::pressure >= Z_THRESHOLD) {
      touchSPI.transfer16(0x91);
      td[0] = touchSPI.transfer16(0xD1) >> 3;
      td[1] = touchSPI.transfer16(0x91) >> 3;
      td[2] = touchSPI.transfer16(0xD1) >> 3;
      td[3] = touchSPI.transfer16(0x91) >> 3;
    }
    td[4] = touchSPI.transfer16(0xD0) >> 3;
    td[5] = touchSPI.transfer16(0)     >> 3;
    digitalWrite(TOUCH_CS, HIGH);
    touchSPI.endTransaction();

    if (LVGL_CYD::pressure < Z_THRESHOLD) { data->state = LV_INDEV_STATE_RELEASED; return; }

    x = besttwoavg(td[0], td[2], td[4]);
    y = besttwoavg(td[1], td[3], td[5]);

    // Map raw ADC values to pixel coordinates (CYD 240×320)
    y = map(y, 200, 3700, TFT_WIDTH - 1, 0);
    y = constrain(y, 0, TFT_WIDTH - 1);
    x = map(x, 200, 3750, 0, TFT_HEIGHT - 1);
    x = constrain(x, 0, TFT_HEIGHT - 1);
    int tmp = x; x = y; y = tmp;  // swap axes to match capacitive orientation
  }

  lv_display_rotation_t rot = lv_display_get_rotation(display);
  if (rot == LV_DISPLAY_ROTATION_90 || rot == LV_DISPLAY_ROTATION_270) {
    x = TFT_WIDTH  - x;
    y = TFT_HEIGHT - y;
  }

  data->point.x = x;
  data->point.y = y;
  data->state   = LV_INDEV_STATE_PRESSED;
}

static int16_t besttwoavg(int16_t a, int16_t b, int16_t c) {
  int16_t dab = abs(a - b), dbc = abs(b - c), dca = abs(c - a);
  if (dab <= dbc && dab <= dca) return (a + b) / 2;
  if (dbc <= dab && dbc <= dca) return (b + c) / 2;
  return (a + c) / 2;
}

#endif  // !ST7796_DRIVER
