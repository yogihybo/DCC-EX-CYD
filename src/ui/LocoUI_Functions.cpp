// LocoUI function-button subsystem — split out of LocoUI.cpp.
// Builds the per-loco function buttons from the loco JSON, lays them out per
// page, and handles their press / paging events. Shared layout constants live
// in LocoUI.h.
#include "LocoUI.h"
#include "Theme.h"
#include <FileSystems.h>
#include <SD.h>
#include <Settings.h>
#include <StreamUtils.h>

LV_FONT_DECLARE(fa_icons_18);

// Encode a Unicode codepoint (U+0800..U+FFFF) as a null-terminated UTF-8 string
static void cp_to_utf8(uint32_t cp, char buf[5]) {
    if (cp < 0x800) {
        buf[0] = 0xC0 | (cp >> 6);
        buf[1] = 0x80 | (cp & 0x3F);
        buf[2] = '\0';
    } else {
        buf[0] = 0xE0 | (cp >> 12);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        buf[3] = '\0';
    }
}

static lv_color_t rgb565_to_lv(uint16_t c) {
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >>  5) & 0x3F;
    uint8_t b5 =  c        & 0x1F;
    return lv_color_make((r5 << 3) | (r5 >> 2),
                         (g6 << 2) | (g6 >> 4),
                         (b5 << 3) | (b5 >> 2));
}

static const char* BUILTIN_PATHS[] = {
    "/www/fns/builtin-basic.json",
    "/www/fns/builtin-extended.json",
};

void LocoUI::buildFunctionButtons(JsonDocument& locoDoc) {
    JsonArrayConst locoFunctions;
    if (locoDoc["functions"].is<JsonArray>()) {
        locoFunctions = locoDoc["functions"].as<JsonArray>();
    } else {
        File functionFile;
        fs::FS& fs = Settings.getFS();
        bool builtinLoaded = false;

        if (locoDoc.containsKey("functions")) {
            const char* fnPath = locoDoc["functions"].as<const char*>();
            if (fnPath && strncmp(fnPath, "builtin:", 8) == 0) {
                int idx = atoi(fnPath + 8);
                if (idx >= 0 && idx < (int)(sizeof(BUILTIN_PATHS) / sizeof(BUILTIN_PATHS[0]))) {
                    File builtinFile = WebsiteFS.open(BUILTIN_PATHS[idx]);
                    if (builtinFile) {
                        StaticJsonDocument<32> filter;
                        filter["functions"] = true;
                        locoDoc.clear();
                        deserializeJson(locoDoc, builtinFile, DeserializationOption::Filter(filter));
                        builtinFile.close();
                        locoFunctions = locoDoc["functions"].as<JsonArray>();
                        builtinLoaded = true;
                    }
                }
            } else if (fnPath && fs.exists(fnPath)) {
                functionFile = fs.open(fnPath);
            }
        }
        if (!builtinLoaded && !functionFile) {
            if (WebsiteFS.exists("/default.json")) {
                functionFile = WebsiteFS.open("/default.json");
            } else if (fs.exists("/default.json")) {
                functionFile = fs.open("/default.json");
            }
        }

        if (functionFile) {
            StaticJsonDocument<32> filter;
            filter["functions"] = true;
            deserializeJson(locoDoc, functionFile, DeserializationOption::Filter(filter));
            locoFunctions = locoDoc["functions"].as<JsonArray>();
            functionFile.close();
        }
    }

    static lv_style_t fn_btn_base_style;
    lv_style_init(&fn_btn_base_style);
    lv_style_set_radius(&fn_btn_base_style, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&fn_btn_base_style, tc(TC_SURFACE_RAISED));
    lv_style_set_border_color(&fn_btn_base_style, tc(TC_BORDER_STRONG));
    lv_style_set_border_width(&fn_btn_base_style, 1);
    lv_style_set_shadow_width(&fn_btn_base_style, 0);
    lv_style_set_shadow_opa(&fn_btn_base_style, LV_OPA_40);

    for (JsonArrayConst const& row : locoFunctions) {
        for (JsonObjectConst const& fn : row) {
            uint8_t func = fn["fn"];
            bool latching = fn["latching"] | true;
            const char* idle_label   = fn["btn"]["idle"]["label"].as<const char*>();
            const char* pressed_label = fn["btn"]["pressed"]["label"].as<const char*>();
            uint32_t idle_cp    = fn["btn"]["idle"]["icon"]    | (uint32_t)0;
            uint32_t pressed_cp = fn["btn"]["pressed"]["icon"] | (uint32_t)0;

            lv_color_t idle_fill   = rgb565_to_lv(fn["btn"]["idle"]["fill"]     | (uint16_t)0x0000);
            lv_color_t idle_border = rgb565_to_lv(fn["btn"]["idle"]["border"]   | (uint16_t)0xFFFF);
            lv_color_t idle_color  = rgb565_to_lv(fn["btn"]["idle"]["color"]    | (uint16_t)0xFFFF);
            lv_color_t press_fill   = rgb565_to_lv(fn["btn"]["pressed"]["fill"]   | (uint16_t)0xFFFF);
            lv_color_t press_border = rgb565_to_lv(fn["btn"]["pressed"]["border"] | (uint16_t)0xFFFF);
            lv_color_t press_color  = rgb565_to_lv(fn["btn"]["pressed"]["color"]  | (uint16_t)0x0000);

            lv_obj_t* btn = lv_btn_create(_container);
            lv_obj_set_size(btn, us(36), us(36));
            lv_obj_add_style(btn, &fn_btn_base_style, 0);
            lv_obj_set_style_bg_color(btn, press_fill, LV_STATE_CHECKED);
            lv_obj_set_style_border_color(btn, press_color, LV_STATE_CHECKED);

            if (latching) {
                lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
            } else {
                lv_obj_set_style_bg_color(btn, press_fill, LV_STATE_PRESSED);
                lv_obj_set_style_border_color(btn, press_color, LV_STATE_PRESSED);
                auto swap_icons = [](lv_event_t* e) {
                    lv_obj_t* b = (lv_obj_t*)lv_event_get_target(e);
                    if (lv_obj_get_child_cnt(b) < 2) return;
                    bool pressing = (lv_event_get_code(e) == LV_EVENT_PRESSED);
                    lv_obj_t* show = lv_obj_get_child(b, pressing ? 1 : 0);
                    lv_obj_t* hide = lv_obj_get_child(b, pressing ? 0 : 1);
                    lv_obj_clear_flag(show, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(hide, LV_OBJ_FLAG_HIDDEN);
                };
                lv_obj_add_event_cb(btn, swap_icons, LV_EVENT_PRESSED,    nullptr);
                lv_obj_add_event_cb(btn, swap_icons, LV_EVENT_RELEASED,   nullptr);
                lv_obj_add_event_cb(btn, swap_icons, LV_EVENT_PRESS_LOST, nullptr);
            }

            auto create_visual = [btn, func](uint32_t cp, const char* label, lv_color_t col) -> lv_obj_t* {
                lv_obj_t* visual_obj = lv_label_create(btn);
                lv_obj_set_style_text_color(visual_obj, col, 0);
                if (cp > 0) {
                    char utf8[5] = {0};
                    cp_to_utf8(cp, utf8);
                    lv_obj_set_style_text_font(visual_obj, &fa_icons_18, 0);
                    lv_label_set_text(visual_obj, utf8);
                } else if (label && strlen(label) > 0) {
                    lv_obj_set_style_text_font(visual_obj, &lv_font_montserrat_10, 0);
                    lv_label_set_text(visual_obj, label);
                } else {
                    lv_obj_set_style_text_font(visual_obj, &lv_font_montserrat_10, 0);
                    lv_label_set_text_fmt(visual_obj, "F%d", func);
                }
                lv_obj_center(visual_obj);
                return visual_obj;
            };

            const uint32_t eff_press_cp    = pressed_cp ? pressed_cp : idle_cp;
            const char*    eff_press_label = (pressed_label && strlen(pressed_label) > 0) ? pressed_label : idle_label;

            lv_obj_t* idle_obj    = create_visual(idle_cp,      idle_label,       idle_color);
            lv_obj_t* pressed_obj = create_visual(eff_press_cp, eff_press_label,  press_color);

            bool is_checked = _loco.functions.test(func);
            if (is_checked) {
                lv_obj_add_state(btn, LV_STATE_CHECKED);
                lv_obj_add_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
            }

            lv_obj_set_user_data(btn, (void*)(uintptr_t)func);
            lv_obj_add_event_cb(btn, fn_btn_event_cb, LV_EVENT_VALUE_CHANGED, this);
            lv_obj_add_event_cb(btn, fn_btn_event_cb, LV_EVENT_CLICKED, this);

            lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
            _fnButtons.push_back(btn);
        }
    }
}

void LocoUI::renderFunctionPage() {
    for (auto btn : _fnButtons) {
        lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    }

    int start_idx = _fnPage * FN_PER_PAGE;
    for (int i = 0; i < FN_PER_PAGE; i++) {
        int idx = start_idx + i;
        if (idx >= (int)_fnButtons.size()) break;

        lv_obj_t* btn = _fnButtons[idx];
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_HIDDEN);

        // Row layout differs per board. The address/nav bar occupies the top of
        // the screen, so the extra 4th row can't go above the 6-button layout's
        // top row without overlapping it. Instead the 4 rows fill the band below
        // the address bar down to the bottom control row, with spacing kept as
        // close to the 6-button version (44/250) as fits (39/250).
        lv_coord_t contentH = (lv_coord_t)(lv_display_get_vertical_resolution(lv_display_get_default()) - 70);
        int rowInCol = i % FN_ROWS;
        int y_pos = (int)(contentH * fnTopOffset() / 250) + rowInCol * (int)(contentH * fnSpacing() / 250);
        if (i < FN_ROWS) lv_obj_align(btn, LV_ALIGN_TOP_LEFT, us(2), y_pos);
        else             lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, us(-2), y_pos);
    }

    if (_pageBtn && _pageBtnLabel) {
        int totalPages = ((int)_fnButtons.size() + FN_PER_PAGE - 1) / FN_PER_PAGE;
        if (totalPages > 1) {
            lv_obj_clear_flag(_pageBtn, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(_pageBtnLabel, "Fn %d/%d", _fnPage + 1, totalPages);
        } else {
            lv_obj_add_flag(_pageBtn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void LocoUI::toggleFunctionButtons(std::bitset<32> toggle) {
    for (auto child : _fnButtons) {
        uint8_t func = (uintptr_t)lv_obj_get_user_data(child);
        if (toggle.test(func)) {
            bool is_checked = _loco.functions.test(func);
            if (is_checked) lv_obj_add_state(child, LV_STATE_CHECKED);
            else            lv_obj_clear_state(child, LV_STATE_CHECKED);
            if (lv_obj_get_child_cnt(child) >= 2) {
                lv_obj_t* idle_obj    = lv_obj_get_child(child, 0);
                lv_obj_t* pressed_obj = lv_obj_get_child(child, 1);
                if (is_checked) {
                    lv_obj_add_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
                } else {
                    lv_obj_add_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }
}


void LocoUI::fn_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    uint8_t func = (uintptr_t)lv_obj_get_user_data(btn);
    bool is_checked = lv_obj_has_state(btn, LV_STATE_CHECKED);

    if (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED) {
        if (lv_obj_get_child_cnt(btn) >= 2) {
            lv_obj_t* idle_obj    = lv_obj_get_child(btn, 0);
            lv_obj_t* pressed_obj = lv_obj_get_child(btn, 1);
            if (is_checked) {
                lv_obj_add_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(pressed_obj, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(idle_obj, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    if (ui->_activeConsist) {
        if (is_checked) ui->_dccex.functionOn(ui->_activeConsist, func);
        else            ui->_dccex.functionOff(ui->_activeConsist, func);
    } else if (ui->_activeLoco) {
        if (is_checked) ui->_dccex.functionOn(ui->_activeLoco, func);
        else            ui->_dccex.functionOff(ui->_activeLoco, func);
    }
}

void LocoUI::page_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    ui->_fnPage++;
    if (ui->_fnPage * FN_PER_PAGE >= (int)ui->_fnButtons.size()) ui->_fnPage = 0;
    ui->renderFunctionPage();
}

