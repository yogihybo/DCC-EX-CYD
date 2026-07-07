#pragma once
#include <lvgl.h>

enum ThemeColor {
    // Surfaces
    TC_SURFACE,          // card / panel background
    TC_SURFACE_RAISED,   // button neutral / elevated element bg
    TC_SURFACE_DEEP,     // tab bar, deeper containers, join button

    // Borders
    TC_BORDER,           // standard container border
    TC_BORDER_STRONG,    // button borders, strong dividers

    // Text
    TC_TEXT_PRIMARY,     // main body text
    TC_TEXT_SECONDARY,   // de-emphasised (icons, sub-labels)
    TC_TEXT_HINT,        // dim labels, placeholders, status text
    TC_TEXT_MUTED,       // very dim (chevrons, inactive elements)

    // Accents
    TC_ACCENT,           // interactive highlight blue
    TC_SECTION,          // teal used for section titles and headings

    // Status indicators (dynamic, called at runtime)
    TC_ICON_ACTIVE,      // active WiFi bar / CS wave
    TC_ICON_INACTIVE,    // inactive bar / wave

    // Badges
    TC_BADGE_BG,         // info badge background (blue-tinted)
    TC_BADGE_FG,         // info badge foreground
    TC_TEAL_BG,          // teal-tinted surface (keypad chips, write button)

    // Overlays / modals
    TC_OVERLAY_BG,       // msgbox / popup background
    TC_OVERLAY_BORDER,   // msgbox border
    TC_OVERLAY_TEXT,     // msgbox body text

    // Danger / destructive actions
    TC_DANGER,           // red background for destructive buttons

    // Speed gauge (lv_scale dial)
    TC_GAUGE_BG,          // matches container background (face + hub blend into screen)
    TC_GAUGE_FACE,       // gauge background fill
    TC_GAUGE_BORDER,     // gauge outer border
    TC_GAUGE_RING,       // scale arc ring
    TC_GAUGE_TICK_MAJOR, // major tick lines
    TC_GAUGE_TICK_MINOR, // minor tick lines
    TC_GAUGE_LABEL,      // tick label text
    TC_GAUGE_NEEDLE,     // needle line
    TC_GAUGE_HUB,        // centre masking circle

    TC_COUNT
};

// ─── UI scale ────────────────────────────────────────────────────────────────
// Hardcoded pixel dimensions are authored against the 2.8" CYD (240x320).
// Larger panels scale those values up proportionally so controls keep the same
// apparent size relative to the screen. Both factors are derived from the
// board's own TFT_WIDTH/TFT_HEIGHT vs the CYD reference, so a new board variant
// scales automatically with no extra #ifdefs.
//
//   us(px)  — height-ratio scale (e.g. 480/320 = 1.5x on the 3.5"). Default for
//             most dimensions; ties vertical layout to the taller screen.
//   usw(px) — width-ratio scale  (e.g. 320/240 = 1.33x on the 3.5"). For controls
//             that must hold the same proportion of screen WIDTH across boards
//             (e.g. the bottom control row: stop / page / direction).
//
// On the CYD both are exactly 1.0x, so that build is unaffected.
#define UI_REF_WIDTH  240
#define UI_REF_HEIGHT 320
static inline int us(int px)  { return px * TFT_HEIGHT / UI_REF_HEIGHT; }
static inline int usw(int px) { return px * TFT_WIDTH  / UI_REF_WIDTH; }

// Returns the default body font for the current board (scaled up on larger panels).
const lv_font_t* ui_font_default();

// Returns the lv_color_t for the given token under the current theme.
lv_color_t tc(ThemeColor color);

// Creates a button with the danger style (red bg, white text, no shadow).
// Returns the button object; label is a child accessible via lv_obj_get_child(btn, 0).
lv_obj_t* make_danger_btn(lv_obj_t* parent, const char* label);
