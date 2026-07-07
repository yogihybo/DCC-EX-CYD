#include "Theme.h"
#include <Settings.h>

static const uint32_t DARK[TC_COUNT] = {
    /* TC_SURFACE        */ 0x1e1e1e,
    /* TC_SURFACE_RAISED */ 0x2e2e2e,
    /* TC_SURFACE_DEEP   */ 0x1a1a1a,
    /* TC_BORDER         */ 0x333333,
    /* TC_BORDER_STRONG  */ 0x555555,
    /* TC_TEXT_PRIMARY   */ 0xeeeeee,
    /* TC_TEXT_SECONDARY */ 0xcccccc,
    /* TC_TEXT_HINT      */ 0x888888,
    /* TC_TEXT_MUTED     */ 0x666666,
    /* TC_ACCENT         */ 0x4488ff,
    /* TC_SECTION        */ 0x26a69a,
    /* TC_ICON_ACTIVE    */ 0xcccccc,
    /* TC_ICON_INACTIVE  */ 0x555555,
    /* TC_BADGE_BG       */ 0x1a2c3e,
    /* TC_BADGE_FG       */ 0x64b5f6,
    /* TC_TEAL_BG        */ 0x1a3a3a,
    /* TC_OVERLAY_BG     */ 0x1e1e1e,
    /* TC_OVERLAY_BORDER */ 0x383838,
    /* TC_OVERLAY_TEXT   */ 0xaaaaaa,
    /* TC_DANGER         */ 0xc83232,
    /* TC_GAUGE_BG       */ 0x1a1a1a,
    /* TC_GAUGE_FACE     */ 0x0b0b0f,
    /* TC_GAUGE_BORDER   */ 0x3a3a4a,
    /* TC_GAUGE_RING     */ 0x555566,
    /* TC_GAUGE_TICK_MAJ */ 0xffffff,
    /* TC_GAUGE_TICK_MIN */ 0x888899,
    /* TC_GAUGE_LABEL    */ 0xffffff,
    /* TC_GAUGE_NEEDLE   */ 0xffffff,
    /* TC_GAUGE_HUB      */ 0x0b0b0f,
};

static const uint32_t LIGHT[TC_COUNT] = {
    /* TC_SURFACE        */ 0xf5f5f5,
    /* TC_SURFACE_RAISED */ 0xffffff,
    /* TC_SURFACE_DEEP   */ 0xe8e8e8,
    /* TC_BORDER         */ 0xdddddd,
    /* TC_BORDER_STRONG  */ 0xbbbbbb,
    /* TC_TEXT_PRIMARY   */ 0x111111,
    /* TC_TEXT_SECONDARY */ 0x444444,
    /* TC_TEXT_HINT      */ 0x666666,
    /* TC_TEXT_MUTED     */ 0x999999,
    /* TC_ACCENT         */ 0x2266cc,
    /* TC_SECTION        */ 0x00897b,
    /* TC_ICON_ACTIVE    */ 0x333333,
    /* TC_ICON_INACTIVE  */ 0xcccccc,
    /* TC_BADGE_BG       */ 0xe3f0fc,
    /* TC_BADGE_FG       */ 0x1a6cb0,
    /* TC_TEAL_BG        */ 0xe0f2f1,
    /* TC_OVERLAY_BG     */ 0xfafafa,
    /* TC_OVERLAY_BORDER */ 0xdddddd,
    /* TC_OVERLAY_TEXT   */ 0x555555,
    /* TC_DANGER         */ 0xc83232,
    /* TC_GAUGE_BG       */ 0xf5f5f5,
    /* TC_GAUGE_FACE     */ 0xf0f0f8,
    /* TC_GAUGE_BORDER   */ 0x9999bb,
    /* TC_GAUGE_RING     */ 0x777788,
    /* TC_GAUGE_TICK_MAJ */ 0x222244,
    /* TC_GAUGE_TICK_MIN */ 0x888899,
    /* TC_GAUGE_LABEL    */ 0x333355,
    /* TC_GAUGE_NEEDLE   */ 0x111133,
    /* TC_GAUGE_HUB      */ 0xf0f0f8,
};

const lv_font_t* ui_font_default() {
    // Body font per board. Flash is near full, so reuse already-enabled sizes
    // (montserrat 14/16) rather than adding new ones. 3.5" gets the larger face.
#if defined(TFT_HEIGHT) && TFT_HEIGHT >= 480
    return &lv_font_montserrat_16;
#else
    return &lv_font_montserrat_14;
#endif
}

lv_color_t tc(ThemeColor color) {
    bool dark = (Settings.theme == SettingsClass::Theme::DARK);
    return lv_color_hex(dark ? DARK[color] : LIGHT[color]);
}

lv_obj_t* make_danger_btn(lv_obj_t* parent, const char* label) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_style_bg_color(btn, tc(TC_DANGER), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xffffff), 0);
    lv_obj_center(lbl);
    return btn;
}
