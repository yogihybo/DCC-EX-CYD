# Adding a New Board Variant

This project supports multiple ESP32 display boards through compile-time build-flag
configuration. Each board gets its own PlatformIO environment in `platformio.ini` and,
if its display or touch hardware differs from existing boards, a new path in the
vendored `src/drivers/LVGL_CYD.cpp`.

---

## Existing board environments

| Environment | Board | Display | Resolution | Touch |
|---|---|---|---|---|
| `esp32-2432S028R` | Sunton CYD 2.8" | ILI9341 / ST7789 (runtime detect) | 240 × 320 | XPT2046 (dedicated VSPI) |
| `ESP32-32E-3_5in-LCD` | LCDWIKI 3.5" | ST7796U | 320 × 480 | XPT2046 (shared HSPI) |

---

## Step-by-step: adding a new board

### 1. Gather hardware details

You need the following before writing any code:

- TFT driver IC (e.g. ILI9341, ST7796, ST7789, GC9A01…)
- Display resolution (width × height in portrait orientation)
- SPI pins for the display: MOSI, MISO, SCLK, CS, DC, RST, backlight (BL)
- Backlight active level: HIGH or LOW
- Touch controller IC (XPT2046, GT911, NS2009…) and its pins
- Whether touch shares the LCD SPI bus or uses a dedicated bus
- Whether the board has PSRAM and how much

The board manufacturer's wiki page (e.g. lcdwiki.com) is usually the most reliable
source for verified pin assignments.

### 2. Add a PlatformIO environment

In `platformio.ini`, add a new `[env:your-board-name]` section that extends `[common]`
and supplies the board-specific build flags.

```ini
[env:your-board-name]
extends = common
build_flags =
    ${common.build_flags}
    ; Display driver — must match a TFT_eSPI driver define (see TFT_eSPI/User_Setups/)
    -DYOUR_DRIVER_DEFINE
    -DTFT_WIDTH=<width>
    -DTFT_HEIGHT=<height>
    -DTFT_BL=<gpio>
    -DTFT_BACKLIGHT_ON=<HIGH|LOW>
    ; TFT SPI pins (if different from the shared defaults in [common])
    ; -DTFT_MOSI=xx -DTFT_MISO=xx -DTFT_SCLK=xx -DTFT_CS=xx -DTFT_DC=xx
    ; Touch pins
    -DTOUCH_MOSI=<gpio>
    -DTOUCH_MISO=<gpio>
    -DTOUCH_CLK=<gpio>
    -DTOUCH_CS=<gpio>
    -DTOUCH_IRQ=<gpio>
    ; Rotary encoder pins
    -DENCODER_CLK_PIN=<gpio>
    -DENCODER_DT_PIN=<gpio>
    -DENCODER_BTN_PIN=<gpio>
    ; If the board has PSRAM:
    ; -DBOARD_HAS_PSRAM
    ; -mfix-esp32-psram-cache-issue
```

**TFT driver defines** come from TFT_eSPI's driver list. Search
`TFT_eSPI/TFT_Drivers/` for your chip (e.g. `ST7796_Driver.h` → `-DST7796_DRIVER`).

The shared TFT SPI pins in `[common]` are MOSI=13, MISO=12, SCLK=14, CS=15, DC=2,
RST=-1 — standard HSPI on the ESP32-D0. Override only what differs.

### 3. Add a board path in `src/drivers/LVGL_CYD.cpp`

Open `src/drivers/LVGL_CYD.cpp` and find the `#if / #elif / #else` block inside
`LVGL_CYD::begin()`. Add a new `#elif` for your driver define.

The block must handle three things:

#### a) Display init (after `lv_tft_espi_create`)

TFT_eSPI sends the driver's init sequence automatically. Only add extra calls if your
display requires inversion, a gamma fix, or a non-standard command sequence.

```cpp
#elif defined(YOUR_DRIVER_DEFINE)
  Serial.printf("[LVGL] YourDisplay %dx%d\n", TFT_WIDTH, TFT_HEIGHT);
  // LVGL_CYD::tft->invertDisplay(true);  // uncomment if colours are inverted
```

#### b) Touch setup

If touch uses a **dedicated SPI bus** (like the CYD): initialise your SPI class and
wire up a read callback similar to `touch_read_cyd`.

If touch **shares the LCD SPI bus**: just detect presence via the IRQ pin and create
a stub `lv_indev`. `main.cpp` owns the `XPT2046_Bitbang` instance and replaces the
callback immediately after `begin()` returns.

```cpp
  // Shared-bus touch detection
  pinMode(TOUCH_IRQ, INPUT_PULLDOWN);
  LVGL_CYD::resistive = digitalRead(TOUCH_IRQ);
  pinMode(TOUCH_IRQ, INPUT);
  if (LVGL_CYD::resistive) {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, [](lv_indev_t*, lv_indev_data_t* d) {
      d->state = LV_INDEV_STATE_RELEASED;
    });
  }
```

#### c) Compile guard around CYD-only code

The resistive touch driver (`touch_read_cyd`, `touchSPI`, `besttwoavg`) is compiled
only when `ST7796_DRIVER` is not defined. Update the compile guard to also exclude your
new board:

```cpp
#if !defined(ST7796_DRIVER) && !defined(YOUR_DRIVER_DEFINE)
// ... CYD touch driver code ...
#endif
```

### 4. Handle PSRAM (if present)

`main.cpp` checks `#ifdef BOARD_HAS_PSRAM` after `LVGL_CYD::begin()` and replaces the
partial draw buffer with a full-screen `ps_malloc` framebuffer in PSRAM. No code change
is needed here — defining `-DBOARD_HAS_PSRAM` and `-mfix-esp32-psram-cache-issue` in
the environment's `build_flags` is sufficient.

### 5. Check for pin conflicts

Common conflicts to look for before finalising pin assignments:

| Signal | Typical GPIO | Conflicts with |
|---|---|---|
| Backlight (BL) | varies | Rotary encoder DT if both assigned the same GPIO |
| Touch IRQ | IO36 | Input-only on ESP32 — cannot drive output |
| Touch CS | IO33 | Must be high before SPI begins |
| SD card CS | IO5 | Hardcoded in `main.cpp` `SD.begin()` call |

If the backlight pin clashes with the encoder, reassign the encoder pin in
`-DENCODER_DT_PIN` (or whichever pin conflicts).

### 6. Verify

Build the new environment with:

```
pio run -e your-board-name
```

Then flash and confirm:

- `[LVGL]` log lines show the correct driver and resolution
- Backlight comes on at boot
- Touch calibration works (Settings → Calibrate)
- Colours look correct (no unexpected inversion)
- Rotary encoder responds if wired

---

## Files changed when adding a board

| File | What to change |
|---|---|
| `platformio.ini` | New `[env:...]` section |
| `src/drivers/LVGL_CYD.cpp` | New `#elif` in `begin()`, update compile guard |
| *(optional)* `src/main.cpp` | Only if the board needs boot-time setup not covered above |

## Credit

`src/drivers/LVGL_CYD.cpp` / `.h` are based on
[LVGL_CYD v1.2.2](https://github.com/ropg/LVGL_CYD) by Rop Gonggrijp, MIT Licence.
The library was vendored into this project to support multiple board variants
without forking the upstream package.
