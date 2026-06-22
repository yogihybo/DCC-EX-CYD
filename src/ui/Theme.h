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

    TC_COUNT
};

// Returns the lv_color_t for the given token under the current theme.
lv_color_t tc(ThemeColor color);

// Creates a button with the danger style (red bg, white text, no shadow).
// Returns the button object; label is a child accessible via lv_obj_get_child(btn, 0).
lv_obj_t* make_danger_btn(lv_obj_t* parent, const char* label);
