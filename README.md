# DCC-EX-CYD Throttle for Cheap Yellow Display (CYD)
<p align="center">
<img width="200" height="208" alt="logo" src="https://github.com/user-attachments/assets/ec05c465-016c-4fdd-a8e6-c6d3c67aa4a7" />
</p>

## Overview
DCC-EX-CYD is port of EX-T3-WiFi that transforms a Cheap Yellow Display (CYD) ESP32-2432S028R into a handheld model railway controller. It communicates asynchronously via WiFi directly to a DCC-EX Command Station. 

The firmware uses **FreeRTOS** and **LVGL 9** to provide a robust and clean UI

<p align="center">
  <img width="240" height="320" alt="interface" src="https://github.com/user-attachments/assets/890e1d9d-f5f0-474c-9332-c6f434f1d76e" />
</p>



### Navigation & Tabs
The interface is split into four primary tabs anchored to the bottom of the screen:
- **Locomotive**: Drive your trains with a touch speedometer, toggle function buttons, swap active locos quickly, and build multi-loco consists.
- **Accessories**: Fast-access control to toggle layout turnouts and accessories using their DCC address.
- **Power**: Control and monitor the DCC-EX Command Station track power (Main, Prog, Join).
- **Settings**: Configure WiFi connections, screen brightness, rotation, touch calibration, and more.


### Web Interface
<p align="center">
<img width="532" height="374" alt="image" src="https://github.com/user-attachments/assets/5bfa0c13-20c4-4d38-b070-d3ac0b6aa48c" />
</p>
A web interface running on the ESP32 allows for easy input of loco details and assigning functions. To enable editting in the web interface, the 'Throttle Programming' mode must be selected on the device screen first.

- **Locos**: Add, edit, and delete locomotives by DCC address and name. Assign a default function set (F0–F9), a saved custom set, or define functions inline with the function editor.
- **Function Sets**: Create reusable function sets with per-button label, colour, latching mode, and icon. Rows can be reordered by drag and drop.
- **Groups**: Organise locos into named groups for quick selection on the throttle. Groups and their members can be reordered by drag and drop.
- **Consists**: Create and manage multi-loco consists. Add members by typing a DCC address directly or selecting from the throttle loco roster. Drag rows to reorder — the first loco (marked **L**) becomes the lead. Each member has a direction toggle (green = forward, yellow = reversed) to set its orientation within the consist. Unknown locos can be named inline. Optionally replicate function button presses to all members. Consists are saved to flash or SD and can be driven or edited directly from the device throttle screen.
- **Icons**: Shows the standard function icons plus the ability to import custom icons
- **Settings**: The settings tab includes wifi and command center settings plus one-click export of all locos, function sets, and groups to a single JSON backup file, and a matching import to restore from it. Storage of the loco roster and functions can be on either the internal ESP32 flash memory or an SD card (selected using setting menu on the device screen) By using the bulk export/import features the roster JSON file can be moved to/from either location.

---

## Hardware Requirements
- **ESP32-2432S028R** (Also known as the CYD / Cheap Yellow Display)
- **Screen**: CYD 2.8" TFT (240x320) with ILI9341 Driver
- **Touch**: CYD Resistive (or Capacitive) touch panel support
- **Power**: USB power or LiPo via additional battery USB charging board (battery voltage/capacity is measured using the touch screen driver IC feature on pin 7)
- **Encoder**: A rotary encoder (optional) to operated as a tactile throttle HW-040
- **Battery and Charging Board**: A LiPo battery and USB LiPo charging board
- **TODO** Details on assembly and case 

### Pin Definitions

#### External — HW-040 Encoder (HW-040 module)

| Function | GPIO | HW-040 Pin | Notes |
|---|---|---|---|
| Encoder CLK (A) | 35 | CLK | Input-only pin; HW-040 supplies pull-up [P3] |
| Encoder DT (B) | 27 | DT | HW-040 supplies pull-up [CN1] |
| Encoder Button (SW) | 22 | SW | Active LOW; ESP32 internal pull-up used (HW-040 SW resistor unpopulated); long-press triggers E-Stop [CN1] |
| 3.3V | 3.3V | + | Encoder power [CN1] |
| GND | GND | GND | Encoder ground [P3 or CN1] |

#### Internal — Hardwired on CYD Board

| Function | GPIO | Notes |
|---|---|---|
| LCD MISO | 12 | Display SPI MISO |
| LCD MOSI | 13 | Display SPI MOSI |
| LCD SCLK | 14 | Display SPI clock |
| LCD CS | 15 | Display chip select |
| LCD DC | 2 | Display data/command select |
| LCD RST | -1 | Not connected |
| Backlight | 21 | PWM backlight control (resistive CYD variant) |
| Touch MISO | 39 | XPT2046 Bit-bang SPI |
| Touch MOSI | 32 | XPT2046 Bit-bang SPI |
| Touch CLK | 25 | XPT2046 Bit-bang SPI |
| Touch CS | 33 | XPT2046 chip select |
| Touch IRQ | 36 | XPT2046 interrupt |
| Red LED | 4 | RGB LED |
| Green LED | 16 | RGB LED |
| Blue LED | 17 | RGB LED |
| SD MISO | 19 | Hardware VSPI [SD card slot] |
| SD MOSI | 23 | Hardware VSPI [SD card slot] |
| SD CLK | 18 | Hardware VSPI [SD card slot] |
| SD CS | 5 | SD chip select [SD card slot] |
| LDR | 34 | Light dependent resistor (unused) |
| XPT2046 VBAT | pin 7 | Battery voltage read via XPT2046 internal ADC channel (÷4 attenuator, 2.5V ref); solder a wire from the JP3 pad (battery voltage source) directly to pin 7 on the XPT2046 IC |

## Software Stack
- **PlatformIO**: Primary build environment and C++ Framework
- **LVGL (v9.1)**: Modern embedded graphics library handling the UI, layouts, widgets, and multi-tab swiping physics.
- **FreeRTOS**: Handles async WiFi keep-alives, background voltage checks, and strictly manages LVGL thread safety via Mutex locks.
- **DCCEXProtocol (v1.3+)**: Official DCC-EX client library handling all Command Station communication — throttle, power, programming track, and accessory commands — via a delegate callback pattern.
- **AsyncTCP**: Low-latency TCP stack used by the web interface.

---

## Core Architecture & Key Functions

The system is constructed around a single, persistent root layout. The core UI modules are loaded once into memory during boot, completely eliminating visual teardowns, loading flickers, and latency. 

### 1. System Initialization & Concurrency (`main.cpp`)
The entry point of the firmware. 
- Initializes the ESP32 hardware, the TFT/CYD drivers, and LVGL.
- Spawns asynchronous FreeRTOS background tasks (`keepWiFiAlive`, `powerCheck`).
- Manages the `DCCEXProtocol` connection lifecycle: a `WiFiClient` TCP connection is established by `keepWiFiAlive` and handed to `DCCEXProtocol::connect()` in `loop()`. A `TeeStream` wrapper intercepts the raw stream to parse Command Station board/shield info without consuming bytes from the library.
- An `AppDelegate` implementing `DCCEXProtocolDelegate` receives all CS callbacks (speed updates, power state, CV read/write results) and dispatches them to the relevant UI module.
- **Thread Safety**: Governs a global `lvgl_mutex` Semaphore. `DCCEXProtocol::check()` and `lv_timer_handler()` are called together inside the same mutex block, so delegate callbacks fire already holding the lock and can safely update LVGL widgets directly.

### 2. Global View Manager (`LVGL_Layouts.cpp / .h`)
A native LVGL container system for the UI.
- **Top Status Bar**: Displays real-time battery voltage, WiFi state, Command Station connection, and active locomotive count utilizing dynamic LVGL symbolic icons (`LV_SYMBOL_WIFI`, `LV_SYMBOL_BATTERY_FULL`, custom Loco and DCC connection icons).
- **Navigation**: Deploys an `lv_tabview` anchored to the bottom of the screen. It seamlessly hosts the 4 permanent sub-applications, enabling native physical swiping between them.

### 3. Loco Control (`LocoUI.cpp`)
The primary dashboard for driving locomotives and consists.
- **Throttle**: Features an `lv_arc` serving as a dynamic rotary speedometer.
- **Function Mapping**: Parses `[address].json` files from LittleFS/SD to dynamically generate a scrolling list of function buttons specific to the active locomotive. A default set (F0–F9) is shown for unrecognised locos.
- **Selection Submenu**: Clicking the active address instantly spawns a hidden overlay popup menu, allowing you to seamlessly swap locomotives via keypad entry, roster name/group, or consist.
- **Direction / E-Stop**: Instant DCC directional toggles and emergency track halts.
- **Consist Mode**: When a consist is active all throttle controls (speed arc, encoder, direction, functions, e-stop) are routed through the DCC-EX consist API. The consist name is shown in place of the loco name.

### 3a. Consist Builder (`ConsistUI.cpp`)
An on-device consist manager accessible from the loco selection menu.
- **Consist List**: Displays all saved consists with member count. Each row has a throttle button (drives the consist immediately) and a settings button (opens the editor).
- **Editor**: Set a name, toggle replicate-functions mode, and manage members with per-loco FWD/REV direction. The lead loco is marked `[L]`. Drive and Save buttons commit the consist; Delete removes the saved file.
- **Add Member**: Numeric keypad entry with a reversed toggle. The first address entered becomes the lead loco.
- **Persistence**: Consists are saved to `/consists/<leadAddr>.json` on LittleFS or SD (whichever is active). Format: `{ name, replicateFunctions, members: [{address, reversed}] }`. Files survive power cycles and reload automatically on next open.

### 4. Accessory / Turnout Manager (`AccessoriesUI.cpp`)
A fast-access manager for layout turnouts and switch machines. Tapping ON/OFF dynamically summons a numeric `lv_keyboard` mapped to an input area, letting you rapidly punch in DCC Accessory Addresses (1-2044) and broadcast their states to the track.

### 5. Track Power (`PowerUI.cpp`)
Receives power state via `AppDelegate::receivedTrackPower` and `receivedIndividualTrackPower` callbacks from the `DCCEXProtocol` library. Features tactile toggle switches to control power across the Main Track, Programming Track, or electronically join them together.

### 6. Throttle Server (`ThrottleServer.cpp`)
A built-in `AsyncWebServer` that hosts the companion web interface for roster management. It is only active when **Throttle Programming** mode is enabled from the Settings tab — on entry the LVGL UI is torn down to reclaim RAM, and rebuilt on exit.

- **REST API**: Handles `GET`, `PUT`, `DELETE`, and `HEAD` requests for loco definitions, function sets, icon images, and groups. Files are streamed directly from LittleFS or SD card via chunked `beginResponse` callbacks — no full file is loaded into RAM.
- **Settings endpoint** (`/cs`): Exposes and accepts WiFi/Command Station configuration as JSON. Save operations are deferred to a short-lived FreeRTOS task to avoid stack overflow in the AsyncTCP context.
- **Backup & Restore**: `GET /backup` streams a full JSON export of the roster; `POST /migrate` copies the roster between internal flash and SD card.
- **CS status** (`HEAD /cs`): Polled by the web UI to indicate whether the device is connected to a Command Station.

### 7. Settings & Network Hub (`SettingsUI.cpp`)
- Controls hardware variables like screen brightness. Includes sub-modules (`WiFiUI.cpp` and `AboutUI.cpp`) that dynamically popup over the settings UI.
--**WiFiUI**; has local AP configuration and local access point mode with QR code, 
--**AboutUI**; tracks live hardware specs and parses Command Station firmware hashes.

---


## Building and Compiling
The project is configured out-of-the-box via `platformio.ini`. 

1. Open the repository in **VSCode** with the **PlatformIO** extension installed.
2. Select the `esp32-2432S028R` environment.
3. Click **Build** and **Upload** to flash your CYD.
4. *Important*: Remember to also run **Upload File System Image** (LittleFS) to upload the necessary loco JSON definitions and system configurations to the ESP32 flash memory. Note: the file system image is flashed to the `website` partition, while the `config` partition is formatted automatically on first boot.

---

## SD Card & Touch Screen SPI Multiplexing

The ESP32 Cheap Yellow Display (CYD) features a notorious hardware conflict: **The SD Card reader and the Resistive Touch Screen are intended to share the same VSPI hardware controller, but are wired to completely different pins.**
- **Touch Pins**: `CLK=25`, `MISO=39`, `MOSI=32`, `CS=33`, `IRQ=36`
- **SD Card Pins**: `CLK=18`, `MISO=19`, `MOSI=23`, `CS=5`

Standard hardware SPI libraries (`SPI.begin()`) cannot easily multiplex between two radically different pin configurations on the fly without triggering core panics or severely degrading performance.

### The Bit-Bang Workaround
To resolve this, this project bypasses the default hardware touch drivers included in `LVGL_CYD`. Instead, the touch screen is driven via a custom **Software SPI (Bit-Bang)** implementation based on [TheNitek/XPT2046_Bitbang_Arduino_Library](https://github.com/TheNitek/XPT2046_Bitbang_Arduino_Library). 
1. **Touch Screen**: Handled entirely in software via standard `digitalRead`/`digitalWrite` pulses. This frees up the hardware VSPI bus completely.
2. **SD Card**: The SD Card reader is granted exclusive access to the hardware VSPI bus (`SD.begin(5, SPI...)`), allowing for high-speed file transfers, formatting, and directory parsing.
3. **Calibration**: Touch calibration coordinates are managed programmatically via the `Settings` menu and scaled dynamically, maintaining native LVGL rotation support.

---

## Custom Icons (LVGL 9)

Generating and importing custom icons into this project requires specific formatting to work properly with LVGL 9 and the C++ linker:

1. **Size Constraints**: Size your icon appropriately (status bar 21x21 pixels, elsewhere 30x30 pixels) *before* converting it. Do not rely on runtime `lv_image_set_scale` for small status bar icons, as it is computationally expensive and can lead to visual artifacts.
2. **Format Generation**: Use an image converter (like the [LVGL Online Image Converter](https://lvgl.github.io/lv_img_conv/)) or generate the C array directly. 
   - Use `LV_COLOR_FORMAT_ARGB8888` for full-color images with transparency.
   - Use `LV_COLOR_FORMAT_A8` (Alpha 8-bit) if you want to dynamically tint/recolor the image at runtime using `lv_obj_set_style_image_recolor()`. This format treats the array as a transparency mask rather than literal color data.
3. **Mandatory Header Fields (The Stride Trap)**: If you generate the array yourself or modify an older LVGL 8 structure, you **must** explicitly define `.header.stride` in the nested `lv_image_dsc_t` header (e.g. `stride = 120` for a 30-pixel wide ARGB8888 image, or `stride = 30` for A8). If you omit this, LVGL 9 defaults the stride to `0`, which silently renders the image with a width of 0 (invisible).
4. **Variable Naming (The Linker Trap)**: When the converter generates the `.c` file, it often uses a default variable name for the structure (like `download` or `image1`). You **must** rename this variable to match your intended usage in the C++ code (e.g., `const lv_image_dsc_t train_icon = {...}`). Failure to do so will result in an `undefined reference` linker error during compilation.
4. **Header Declaration**: In your `.h` file, declare the icon struct using `extern "C"` so the C++ linker can find the C-compiled array:
   ```cpp
   #ifdef __cplusplus
   extern "C" {
   #endif
   extern const lv_image_dsc_t train_icon;
   #ifdef __cplusplus
   }
   #endif
   ```
