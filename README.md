# DCC-EX-CYD — WiFi Throttle for ESP32 Touch Displays (CYD 2.8" & LCDWIKI 3.5")
<p align="center">
<img width="200" height="208" alt="logo" src="https://github.com/user-attachments/assets/ec05c465-016c-4fdd-a8e6-c6d3c67aa4a7" />
</p>

## Overview
DCC-EX-CYD is a port of EX-T3-WiFi that transforms an inexpensive ESP32 touch-display board into a handheld model railway controller. It communicates asynchronously via WiFi directly to a DCC-EX Command Station.

The firmware uses **FreeRTOS** and **LVGL 9** to provide a robust and clean UI.

### Supported Boards
The firmware supports multiple board variants, each selected as its own PlatformIO environment. The UI scales automatically to the panel size, so the same features are available on both.

| Board | Env (`platformio.ini`) | Display | Resolution | Touch | Fn buttons / page |
|---|---|---|---|---|---|
| **ESP32-2432S028R** (Cheap Yellow Display / CYD) | `esp32-2432S028R` | 2.8" ILI9341 | 240×320 | XPT2046 (bit-bang SPI) | 6 (2×3) |
| **LCDWIKI 3.5" ESP32-32E** | `3inch5-ESP32-32E` | 3.5" ST7796 | 320×480 | XPT2046 (shared LCD SPI bus) | 8 (2×4) |

Board-specific pins, display driver, touch wiring, and UI scale are all set via build flags in each environment — no code changes are needed to switch boards. See [Board Differences](#board-differences) for details.

<p align="center">
<img width="240" height="320" alt="demo" src="https://github.com/user-attachments/assets/fd9401f1-40ce-46f9-9039-7acc50f2ab25" />

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

- **Locos**: Add, edit, and delete locomotives by DCC address and name. Assign one of the two built-in default function sets (Basic F0–F8 or Extended F0–F15), a saved custom set, or define functions inline with the function editor.
- **Function Sets**: Create reusable function sets with per-button label, colour, latching mode, and icon. Rows can be reordered by drag and drop. Two built-in read-only defaults are included (shown with a lock badge):
  - **Default — Basic (F0–F8)**: Headlights, Bell, Horn, Dynamic Brake, Ditch Lights, Cab Light, Strobe, Coupler, Mute. Horn, Coupler are momentary; all others latching.
  - **Default — Extended (F0–F15)**: Extends Basic with Cooling Fan, Prime Mover, Air Compressor, Brake Squeal, Smoke Unit, Firebox, and Injector. Brake Squeal is momentary.
  
  Built-in sets cannot be edited or deleted. Use the **Copy** button to duplicate one into an editable custom set.
- **Groups**: Organise locos into named groups for quick selection on the throttle. Groups and their members can be reordered by drag and drop.
- **Consists**: Create and manage multi-loco consists. Add members by typing a DCC address directly or selecting from the throttle loco roster. Drag rows to reorder — the first loco (marked **L**) becomes the lead. Each member has a direction toggle (green = forward, yellow = reversed) to set its orientation within the consist. Unknown locos can be named inline. Optionally replicate function button presses to all members. Consists are saved to flash or SD and can be driven or edited directly from the device throttle screen.
- **Icons**: Shows the standard function icons plus the ability to import custom icons
- **Settings**: The settings tab includes wifi and command center settings plus one-click export of all locos, function sets, and groups to a single JSON backup file, and a matching import to restore from it. Storage of the loco roster and functions can be on either the internal ESP32 flash memory or an SD card (selected using setting menu on the device screen) By using the bulk export/import features the roster JSON file can be moved to/from either location.

---

## Hardware Requirements
One of the supported ESP32 display boards:
- **ESP32-2432S028R** — the CYD / Cheap Yellow Display: 2.8" ILI9341 TFT (240×320), resistive (or capacitive) touch.
- **LCDWIKI 3.5" ESP32-32E** — 3.5" ST7796 TFT (320×480), XPT2046 resistive touch sharing the LCD SPI bus. [Product page](https://www.lcdwiki.com/3.5inch_ESP32-32E_Display).

Common to both:
- **Power**: USB power or a LiPo via a USB LiPo charging board.
- **Encoder** *(optional)*: an HW-040 rotary encoder acting as a tactile throttle; the throttle can also be driven entirely by touch.
- **TODO**: details on assembly and case.

Board-specific notes:
- **CYD** measures battery voltage through the XPT2046 touch IC's VBAT channel (pin 7).
- **3.5" ESP32-32E** measures battery voltage on GPIO 34 through an on-board 1:1 (100k/100k) divider, and has no PSRAM.

---

### CYD (ESP32-2432S028R) Pin Definitions

#### External — HW-040 Encoder (entirely optional as throttle can be controlled on screen by touch input)

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

---

### 3.5" ESP32-32E Pin Definitions

On this board the XPT2046 touch controller **shares the LCD SPI bus** (GPIO 12/13/14) rather than using a separate bit-bang bus. The firmware drives touch through TFT_eSPI's built-in XPT2046 support (with CS switching) so the shared bus is not corrupted.

#### External — HW-040 Encoder (optional)

| Function | GPIO | Notes |
|---|---|---|
| Encoder CLK (A) | 32 | Free GPIO with internal pull |
| Encoder DT (B) | 25 | Free GPIO with internal pull |
| Encoder Button (SW) | 23 | Active LOW; long-press triggers E-Stop. Shared with the SD-card MOSI line — fine unless an SD card is used simultaneously |
| 3.3V / GND | — | Encoder power/ground |

#### Internal — Hardwired on the 3.5" Board

| Function | GPIO | Notes |
|---|---|---|
| LCD MISO | 12 | Display SPI MISO (shared with touch) |
| LCD MOSI | 13 | Display SPI MOSI (shared with touch) |
| LCD SCLK | 14 | Display SPI clock (shared with touch) |
| LCD CS | 15 | Display chip select |
| LCD DC | 2 | Display data/command select |
| LCD RST | -1 | Not connected |
| Backlight | 27 | PWM backlight (active HIGH) |
| Touch CS | 33 | XPT2046 chip select |
| Touch IRQ | 36 | XPT2046 interrupt (input-only pad) |
| Red LED | 22 | RGB LED red channel |
| Battery ADC | 34 | Battery voltage via on-board 1:1 (100k/100k) divider; read with `analogReadMilliVolts` ×2 |
| SD MISO / MOSI / CLK / CS | 19 / 23 / 18 / 5 | Hardware VSPI [SD card slot] |

> **Note**: GPIO 34–39 are input-only pads with no internal pull resistors, so they are used only for the touch IRQ and battery ADC. Display resolution, driver, backlight polarity, RGB order, touch pins, encoder pins, and battery pin are all set as build flags in the `[env:3inch5-ESP32-32E]` section of `platformio.ini`.

---

<a name="board-differences"></a>
### Board Differences

| Area | CYD (ESP32-2432S028R) | 3.5" ESP32-32E |
|---|---|---|
| Display driver | ILI9341 / ST7789 (auto-detected) | ST7796 |
| Resolution | 240×320 | 320×480 |
| Touch bus | XPT2046 on a dedicated software (bit-bang) SPI bus | XPT2046 on the shared LCD SPI bus (TFT_eSPI built-in driver, CS switching) |
| Battery sense | XPT2046 VBAT (pin 7) | GPIO 34 + 1:1 divider |
| PSRAM | n/a | none (4 MB is flash only) |
| RGB LED | GPIO 4/16/17 | Red on GPIO 22 |
| Function buttons/page | 6 (2×3) | 8 (2×4) |
| UI scale | reference (1.0×) | ~1.5× (height) via `us()`; bottom controls use ~1.33× (width) via `usw()` |

The UI is authored against the CYD's pixel dimensions. `Theme.h` provides `us()` (height-ratio) and `usw()` (width-ratio) scale helpers, plus `ui_font_default()`, so hardcoded sizes, paddings and fonts scale up on the larger panel while the CYD build stays byte-for-byte identical (scale factor 1/1). The status/header bar is deliberately left un-scaled so it reads the same on both boards.

## Software Stack
- **PlatformIO**: Primary build environment and C++ Framework
- **FreeRTOS**: Handles async WiFi keep-alives, background voltage checks, and strictly manages LVGL thread safety via Mutex locks.
- **AsyncTCP**: Low-latency TCP stack used by the web interface.
- **LVGL (v9.1)**: Modern embedded graphics library handling the UI, layouts, widgets, and multi-tab swiping physics.
- **DCCEXProtocol (v1.3+)**: Official DCC-EX client library handling all Command Station communication — throttle, power, programming track, and accessory commands — via a delegate callback pattern.


---

## Core Architecture & Key Functions

The system is constructed around a single, persistent root layout. The core UI modules are loaded once into memory during boot, completely eliminating visual teardowns, loading flickers, and latency.

### 1. System Initialization & Concurrency (`main.cpp`)
The entry point of the firmware.
- Initializes the ESP32 hardware, the TFT/CYD drivers, and LVGL.
- Spawns asynchronous FreeRTOS background tasks (`keepWiFiAlive`, `powerCheck`).
- Manages the `DCCEXProtocol` connection lifecycle: a `WiFiClient` TCP connection is established by `keepWiFiAlive` and handed to `DCCEXProtocol::connect()` in `loop()`. A `TeeStream` wrapper intercepts the raw stream to parse Command Station board/shield info without consuming bytes from the library.
- An `AppDelegate` implementing `DCCEXProtocolDelegate` receives all CS callbacks (speed updates, power state, CV read/write results, turnout state changes) and dispatches them to the relevant UI module.
- **Thread Safety**: Governs a global `lvgl_mutex` Semaphore. `DCCEXProtocol::check()` and `lv_timer_handler()` are called together inside the same mutex block, so delegate callbacks fire already holding the lock and can safely update LVGL widgets directly.

### 2. Global View Manager (`LVGL_Layouts.cpp / .h`)
A native LVGL container system for the UI.
- **Top Status Bar**: Displays real-time battery voltage, WiFi state, Command Station connection, and active locomotive count utilizing dynamic LVGL symbolic icons (`LV_SYMBOL_WIFI`, `LV_SYMBOL_BATTERY_FULL`, custom Loco and DCC connection icons). The DCC status icon renders as a live square-wave waveform using an `lv_line` polyline — green when connected, red when not.
- **Rear RGB LED**: The onboard RGB LED (active-LOW; GPIO 4/16/17 on the CYD, red on GPIO 22 for the 3.5") mirrors the Command Station connection state — green when connected, red when disconnected.
- **Navigation**: Deploys an `lv_tabview` anchored to the bottom of the screen. It seamlessly hosts the 4 permanent sub-applications, enabling native physical swiping between them.

### 3. Loco Control (`LocoUI.cpp`)
The primary dashboard for driving locomotives and consists.
- **Throttle**: Features an `lv_arc` serving as a dynamic rotary speedometer. Speed updates from the CS are accepted unless a local change was made within the last 500 ms (preventing CS echo fighting user input).
- **Function Mapping**: Parses `[address].json` files from LittleFS/SD to dynamically generate function buttons specific to the active locomotive — 6 per page (2×3) on the CYD, 8 per page (2×4) on the taller 3.5" panel, with a paging button when a loco has more than fits on one page. Each button supports idle and pressed visual states (label, icon, colour) defined per-function in the JSON. Latching buttons toggle on each press; momentary buttons animate on press and release independently of the DCC function state roundtrip. On the 3.5" the throttle dial is vertically centred on the mid-line of the function-button block.
- **Selection Menu**: Tapping the active address opens a dark-themed overlay panel with options to select a locomotive by address (numeric keypad), by name (roster list), by group, or by consist. A Release button stops and deregisters the current loco or consist.
- **Direction / E-Stop**: Instant DCC directional toggle (blocked at speed > 0) and a prominent circular emergency stop button.
- **Consist Mode**: When a consist is active all throttle controls (speed arc, encoder, direction, functions, e-stop) are routed through the DCC-EX consist API. The consist name is shown in place of the loco name.

### 3a. Consist Builder (`ConsistUI.cpp`)
An on-device consist manager accessible from the loco selection menu.
- **Consist List**: Displays all saved consists with member count. Each row has a throttle button (drives the consist immediately) and a settings button (opens the editor).
- **Editor**: Set a name, toggle replicate-functions mode, and manage members with per-loco FWD/REV direction. The lead loco is marked `[L]`. Drive and Save buttons commit the consist; Delete removes the saved file.
- **Add Member**: Numeric keypad entry with a reversed toggle. The first address entered becomes the lead loco.
- **Persistence**: Consists are saved to `/consists/<leadAddr>.json` on LittleFS or SD (whichever is active). Format: `{ name, replicateFunctions, members: [{address, reversed}] }`. Files survive power cycles and reload automatically on next open.

### 4. Accessory / Turnout Manager (`AccessoriesUI.cpp`)
A fast-access manager for layout turnouts and switch machines.
- **Address Entry**: A numeric `lv_keyboard` is summoned from `lv_layer_top()` (full-screen dock) when the address field is focused, and dismissed on confirm or cancel. Accepts DCC accessory addresses 1–2044.
- **Status Query**: When an address is confirmed (keyboard or recent chip), the throttle sends `<T>` to the Command Station. If the entered number matches a defined turnout ID, the CS responds with its current state (`<H id state>`) which is received via `receivedTurnoutAction` and reflected immediately in the Throw/Close button highlights — without requiring the user to send a command first.
- **Throw / Close**: Dedicated buttons send the turnout command immediately via `activateLinearAccessory` / `deactivateLinearAccessory`. The active state is highlighted with semantic colour (amber = thrown, green = closed); the inactive button returns to neutral dark.
- **Recent Addresses**: The last 5 addresses used are shown as a chip strip below the input. Tapping a chip populates the address field and triggers a status query. Chips highlight blue on press.

### 5. Track Power (`PowerUI.cpp`)
Per-track power control with real-time visual state feedback.
- **Layout**: Two dark cards (Main Track, Prog Track) each showing a coloured dot indicator, ON/OFF status text, and ON/OFF buttons. A shared All ON / All OFF row and a Join Tracks button sit below.
- **State**: Power state is updated optimistically on button press (since the CS does not always echo a broadcast). CS broadcasts via `receivedTrackPower` and `receivedIndividualTrackPower` override the local state when received.
- **Dot Indicators**: 10×10 px circular `lv_obj` widgets coloured green (ON) or red (OFF) — not Unicode glyphs, which are absent from the Montserrat font set.
- **Join Tracks**: Electronically bridges Main and Prog tracks via `joinProg()`. The button toggles to "Unjoin (cuts power)" when active; pressing it again calls `powerOff()` and resets all state to OFF.
- **Semantic Colours**: Green `(40,140,40)` = ON/active, muted red `(140,40,40)` = OFF, amber `(180,120,30)` = joined.

### 6. CV Programmer (`ProgramUI.cpp`)
A multi-step CV programming interface for the DCC programming track.
- **Sections**: Address (read/write loco address), CV Operations (read/write byte, read/write bit), and a de-emphasised Advanced section (ACK Limit / Min / Max diagnostic tuning).
- **Common CV Presets**: A row of shortcut chips — Address (CV1), Accel (CV3), Decel (CV4), V-Max (CV5), Steps (CV29) — reads the CV immediately without requiring manual number entry.
- **Read-then-Write Flow**: After a successful CV read, the result popup shows the current value and offers a "Write new value" button, enabling the common read-modify-write workflow in one continuous flow.
- **Step Context**: Multi-step prompts carry forward context ("CV 3 — enter value") so the user always knows which CV they are editing.
- **Read Bit Fix**: Read Bit calls `readCV(cv)` and extracts `(byte >> bit) & 1` from the full byte result, rather than the previous implementation which only validated the bit against 1.
- **Timeouts**: A 10-second timer fires if the CS does not respond. Error messages distinguish "No ACK — check loco is on prog track" from write failures.
- **Styled Popups**: All intermediate dialogs (Working, result, confirm, input) use the shared dark `0x1e1e1e` popup style with teal titles.

### 7. Throttle Server (`ThrottleServer.cpp`)
A built-in `AsyncWebServer` that hosts the companion web interface for roster management. It is only active when **Throttle Programming** mode is enabled from the Settings tab — on entry the LVGL UI is torn down to reclaim RAM, and rebuilt on exit.

- **REST API**: Handles `GET`, `PUT`, `DELETE`, and `HEAD` requests for loco definitions, function sets, icon images, and groups. Files are streamed directly from LittleFS or SD card via chunked `beginResponse` callbacks — no full file is loaded into RAM.
- **Settings endpoint** (`/cs`): Exposes and accepts WiFi/Command Station configuration as JSON. Save operations are deferred to a short-lived FreeRTOS task to avoid stack overflow in the AsyncTCP context.
- **Backup & Restore**: `GET /backup` streams a full JSON export of the roster; `POST /migrate` copies the roster between internal flash and SD card.
- **CS status** (`HEAD /cs`): Polled by the web UI to indicate whether the device is connected to a Command Station.

### 8. Settings & Network Hub (`SettingsUI.cpp`)
Dark-themed settings panel with teal section headings and value badges. Sections include Throttle (brightness, rotation, encoder acceleration) and CS & Storage (WiFi, SD format, touch calibration, about).
- **Brightness**: Inline popup with a styled 4 px-track slider; adjusts backlight PWM in real time and persists to flash.
- **Rotation**: Cycles between USB Down (portrait) and USB Up (landscape) with immediate LVGL rotation.
- **WiFiUI**: Full-screen scrollable overlay for network credentials and Command Station address/port. A numeric keyboard is shown in-context when a field is focused, and the container bottom padding expands to keep the active field above the keyboard. Includes AP SSID/password and a QR code for easy mobile connection.
- **AboutUI**: Live hardware specs and Command Station firmware info, organised into sections — Throttle, Connection, SD Card, and DCC-EX Command Station.
- **mDNS**: If the Command Station hostname contains no `.` it is treated as a `.local` mDNS name. `MDNS.begin("dcc-ex-cyd")` is also started so the throttle itself is reachable at `dcc-ex-cyd.local`.

### UI Design System
All screens share a consistent dark theme:
- **Background**: `0x1e1e1e` cards on a default dark LVGL surface
- **Section headings**: Teal `rgb(38,166,154)` labels
- **Buttons**: `0x2e2e2e` neutral, green `(40,140,40)` active/ON, muted red `(140,40,40)` OFF/destructive, amber `(180,120,30)` thrown/joined
- **Popups**: `0x1e1e1e` background, `0x383838` 1 px border, teal title text
- **Consistent inset**: 5 px container padding + 6 px inner padding = 11 px total horizontal margin across all tabs

---


## Building and Compiling
The project is configured out-of-the-box via `platformio.ini`. 

1. Open the repository in **VSCode** with the **PlatformIO** extension installed.
2. Select the environment for your board:
   - `esp32-2432S028R` for the 2.8" CYD.
   - `3inch5-ESP32-32E` for the 3.5" LCDWIKI board.
3. Click **Build** and **Upload** to flash the device.
4. *Important*: Remember to also run **Upload File System Image** (LittleFS) to upload the necessary loco JSON definitions and system configurations to the ESP32 flash memory. Note: the file system image is flashed to the `website` partition, while the `config` partition is formatted automatically on first boot.
5. On first boot, run **Touch Calibration** from the Settings tab so touch coordinates match your panel (the default calibration values differ per board).

---

## SD Card & Touch Screen SPI Multiplexing

> This section applies to the **CYD (ESP32-2432S028R)**. On the **3.5" ESP32-32E** the XPT2046 touch controller shares the LCD SPI bus directly and is driven through TFT_eSPI's built-in XPT2046 support (hardware SPI with CS switching) — the bit-bang workaround below is compiled out for that board via the `ST7796_DRIVER` flag. Reading touch through TFT_eSPI's `getTouchRaw()`/`getTouchRawZ()` avoids reconfiguring the shared bus pins as GPIO, which would otherwise corrupt the display.

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
