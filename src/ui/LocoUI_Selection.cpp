// LocoUI selection subsystem — split out of LocoUI.cpp.
// The loco selection menu (by address / name / group / consist / recent), the
// numeric keypad, and all of their event callbacks.
#include "LocoUI.h"
#include "Theme.h"
#include <FileSystems.h>
#include <SD.h>
#include <Settings.h>
#include <StreamUtils.h>

void LocoUI::buildSelectionMenu() {
    _selectionMenu = lv_obj_create(_container);
    lv_obj_set_width(_selectionMenu, LV_PCT(90));
#if defined(TFT_HEIGHT) && TFT_HEIGHT >= 480
    // 3.5": the taller panel fits everything — size the menu snugly to its
    // content (capped) so it never scrolls or bounces.
    lv_obj_set_height(_selectionMenu, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(_selectionMenu, LV_PCT(95), 0);
#else
    // CYD: content can exceed the small screen; keep a capped height and scroll.
    lv_obj_set_height(_selectionMenu, LV_PCT(95));
#endif
    lv_obj_align(_selectionMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_selectionMenu, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_color(_selectionMenu, tc(TC_OVERLAY_BORDER), 0);
    lv_obj_set_style_border_width(_selectionMenu, 1, 0);
    lv_obj_set_style_radius(_selectionMenu, us(8), 0);
    lv_obj_set_style_pad_all(_selectionMenu, us(8), 0);
    lv_obj_set_style_pad_row(_selectionMenu, us(5), 0);
    lv_obj_set_flex_flow(_selectionMenu, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_selectionMenu, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
#if defined(TFT_HEIGHT) && TFT_HEIGHT >= 480
    // 3.5": no scrolling — content is sized to fit.
    lv_obj_clear_flag(_selectionMenu, LV_OBJ_FLAG_SCROLLABLE);
#else
    // CYD: allow vertical scrolling so the Recent chips can't clip Release.
    lv_obj_set_scroll_dir(_selectionMenu, LV_DIR_VER);
#endif
    lv_obj_add_flag(_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title_row = lv_obj_create(_selectionMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, us(30));
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select Locomotive");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, us(28));
    lv_obj_set_style_pad_hor(close_btn, us(10), 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, close_selection_event_cb, LV_EVENT_CLICKED, this);

    auto make_menu_btn = [&](const char* label, lv_event_cb_t cb, lv_color_t bg) {
        lv_obj_t* btn = lv_btn_create(_selectionMenu);
        lv_obj_set_width(btn, LV_PCT(100));
        lv_obj_set_height(btn, us(30));
        lv_obj_set_style_bg_color(btn, bg, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_center(lbl);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, this);
    };

    make_menu_btn("By Address", addr_btn_event_cb,    tc(TC_SURFACE_RAISED));
    make_menu_btn("By Name",    name_btn_event_cb,    tc(TC_SURFACE_RAISED));
    make_menu_btn("By Group",   group_btn_event_cb,   tc(TC_SURFACE_RAISED));
    make_menu_btn("By Consist", consist_btn_event_cb, tc(TC_SURFACE_RAISED));

    // Recent locos — quick-recall chips (persisted across sessions)
    if (Settings.recentLocoCount > 0) {
        lv_obj_t* rec_lbl = lv_label_create(_selectionMenu);
        lv_label_set_text(rec_lbl, "Recent");
        lv_obj_set_style_text_color(rec_lbl, tc(TC_TEXT_HINT), 0);
        lv_obj_set_style_text_font(rec_lbl, &lv_font_montserrat_10, 0);

        lv_obj_t* rec_row = lv_obj_create(_selectionMenu);
        lv_obj_set_width(rec_row, LV_PCT(100));
        lv_obj_set_height(rec_row, LV_SIZE_CONTENT);
        lv_obj_set_style_max_height(rec_row, us(72), 0);
        lv_obj_set_style_pad_all(rec_row, 0, 0);
        lv_obj_set_style_pad_row(rec_row, us(4), 0);
        lv_obj_set_style_pad_column(rec_row, us(4), 0);
        lv_obj_set_style_border_width(rec_row, 0, 0);
        lv_obj_set_style_bg_opa(rec_row, 0, 0);
        lv_obj_set_flex_flow(rec_row, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_flex_align(rec_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

        for (uint8_t i = 0; i < Settings.recentLocoCount; i++) {
            uint16_t addr = Settings.recentLocos[i];
            lv_obj_t* chip = lv_btn_create(rec_row);
            lv_obj_set_height(chip, us(26));
            lv_obj_set_style_pad_hor(chip, us(8), 0);
            lv_obj_set_style_pad_ver(chip, 0, 0);
            lv_obj_set_style_radius(chip, us(13), 0);
            lv_obj_set_style_bg_color(chip, tc(TC_SURFACE_RAISED), 0);
            lv_obj_set_style_shadow_width(chip, 0, 0);
            lv_obj_set_user_data(chip, (void*)(uintptr_t)addr);
            lv_obj_add_event_cb(chip, recent_loco_event_cb, LV_EVENT_CLICKED, this);
            lv_obj_t* cl = lv_label_create(chip);
            lv_label_set_text_fmt(cl, "%d", addr);
            lv_obj_set_style_text_font(cl, &lv_font_montserrat_12, 0);
            lv_obj_center(cl);
        }
    }

    lv_obj_t* rel_btn = make_danger_btn(_selectionMenu, "Release");
    lv_obj_set_width(rel_btn, LV_PCT(100));
    lv_obj_set_height(rel_btn, us(30));
    lv_obj_add_event_cb(rel_btn, release_btn_event_cb, LV_EVENT_CLICKED, this);
}

void LocoUI::showKeypad() {
  if (!_keyboard) {
    _keyboard = lv_obj_create(_container);
    lv_obj_set_size(_keyboard, LV_PCT(100), LV_PCT(100));
    lv_obj_align(_keyboard, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_keyboard, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_width(_keyboard, 0, 0);
    lv_obj_set_style_pad_all(_keyboard, us(8), 0);
    lv_obj_set_style_pad_row(_keyboard, us(5), 0);
    lv_obj_set_flex_flow(_keyboard, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(_keyboard);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, us(36));
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Address");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, us(28));
    lv_obj_set_style_pad_hor(close_btn, us(10), 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);

    _textarea = lv_textarea_create(_keyboard);
    lv_obj_set_width(_textarea, LV_PCT(100));
    lv_textarea_set_one_line(_textarea, true);
    lv_textarea_set_placeholder_text(_textarea, "Loco Address");

    lv_obj_t* kb = lv_keyboard_create(_keyboard);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(kb, _textarea);
    lv_obj_set_flex_grow(kb, 1);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, this);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, this);

    lv_obj_add_event_cb(close_btn, [](lv_event_t* e) {
        lv_obj_t* kb = (lv_obj_t*)lv_event_get_user_data(e);
        lv_obj_send_event(kb, LV_EVENT_CANCEL, NULL);
    }, LV_EVENT_CLICKED, kb);
  }
}

void LocoUI::hideKeypad() {
  if (_keyboard) { lv_obj_delete_async(_keyboard); _keyboard = nullptr; }
  if (_textarea) { lv_obj_delete_async(_textarea); _textarea = nullptr; }
}

void LocoUI::open_selection_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_move_foreground(ui->_selectionMenu);
    lv_obj_clear_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
}

void LocoUI::close_selection_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
}

void LocoUI::name_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    if (ui->_nameMenu) {
        lv_obj_delete_async(ui->_nameMenu);
        ui->_nameMenu = nullptr;
    }

    ui->_nameMenu = lv_obj_create(ui->_container);
    lv_obj_set_size(ui->_nameMenu, LV_PCT(90), LV_PCT(95));
    lv_obj_align(ui->_nameMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui->_nameMenu, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_color(ui->_nameMenu, tc(TC_OVERLAY_BORDER), 0);
    lv_obj_set_style_border_width(ui->_nameMenu, 1, 0);
    lv_obj_set_style_radius(ui->_nameMenu, us(8), 0);
    lv_obj_set_style_pad_all(ui->_nameMenu, us(8), 0);
    lv_obj_set_style_pad_row(ui->_nameMenu, us(5), 0);
    lv_obj_set_flex_flow(ui->_nameMenu, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, us(36));
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Name");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, us(28));
    lv_obj_set_style_pad_hor(close_btn, us(10), 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, close_name_menu_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t* list = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, us(4), 0);
    lv_obj_set_style_border_width(list, 0, 0);

    auto addLocos = [ui, list](fs::FS& fs) {
        File dir = fs.open("/locos");
        if (dir) {
            StaticJsonDocument<16> filterDoc;
            filterDoc["name"] = true;
            StaticJsonDocument<64> locoDoc;
            while (File file = dir.openNextFile()) {
                if (!file.isDirectory()) {
                    uint16_t address = strtoul(file.name(), (char**)NULL, 10);

                    locoDoc.clear();
                    ReadBufferingStream buffered(file, 64);
                    deserializeJson(locoDoc, buffered, DeserializationOption::Filter(filterDoc));

                    char nameStr[56];
                    const char* locoName = locoDoc["name"] | "Unknown";
                    snprintf(nameStr, sizeof(nameStr), "#%d - %s", address, locoName);

                    lv_obj_t* btn = lv_btn_create(list);
                    lv_obj_set_width(btn, LV_PCT(100));
                    lv_obj_set_height(btn, us(36));
                    lv_obj_set_style_bg_color(btn, tc(TC_SURFACE_RAISED), 0);
                    lv_obj_set_style_shadow_width(btn, 0, 0);
                    lv_obj_t* lbl = lv_label_create(btn);
                    lv_label_set_text(lbl, nameStr);
                    lv_obj_center(lbl);

                    lv_obj_set_user_data(btn, (void*)(uintptr_t)address);
                    lv_obj_add_event_cb(btn, loco_selected_event_cb, LV_EVENT_CLICKED, ui);
                }
                file.close();
            }
            dir.close();
        }
    };

    addLocos(Settings.getFS());
}

void LocoUI::loco_selected_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    uint16_t address = (uintptr_t)lv_obj_get_user_data(btn);

    if (address > 0 && address <= 9999) {
        ui->_locos.add(address);
        Settings.pushRecentLoco(address);
        ui->_nameMenu = nullptr;
        ui->_groupsDoc.reset();
        lv_async_call([](void* user_data) {
            ((LocoUI*)user_data)->refresh();
        }, ui);
    }
}

void LocoUI::recent_loco_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* chip = (lv_obj_t*)lv_event_get_target(e);
    uint16_t addr = (uintptr_t)lv_obj_get_user_data(chip);
    if (addr == 0) return;

    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
    ui->_locos.add(addr);
    Settings.pushRecentLoco(addr);
    lv_async_call([](void* user_data) {
        ((LocoUI*)user_data)->refresh();
    }, ui);
}

void LocoUI::close_name_menu_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_nameMenu) {
        lv_obj_delete_async(ui->_nameMenu);
        ui->_nameMenu = nullptr;
    }
    ui->_groupsDoc.reset();
    if (ui->_selectionMenu) lv_obj_clear_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
}

void LocoUI::group_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    if (ui->_nameMenu) {
        lv_obj_delete_async(ui->_nameMenu);
        ui->_nameMenu = nullptr;
    }

    ui->_nameMenu = lv_obj_create(ui->_container);
    lv_obj_set_size(ui->_nameMenu, LV_PCT(90), LV_PCT(95));
    lv_obj_align(ui->_nameMenu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui->_nameMenu, tc(TC_OVERLAY_BG), 0);
    lv_obj_set_style_border_color(ui->_nameMenu, tc(TC_OVERLAY_BORDER), 0);
    lv_obj_set_style_border_width(ui->_nameMenu, 1, 0);
    lv_obj_set_style_radius(ui->_nameMenu, us(8), 0);
    lv_obj_set_style_pad_all(ui->_nameMenu, us(8), 0);
    lv_obj_set_style_pad_row(ui->_nameMenu, us(5), 0);
    lv_obj_set_flex_flow(ui->_nameMenu, LV_FLEX_FLOW_COLUMN);

    lv_obj_t* title_row = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, us(36));
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "Select By Group");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* close_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(close_btn, LV_SIZE_CONTENT, us(28));
    lv_obj_set_style_pad_hor(close_btn, us(10), 0);
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(close_btn, close_name_menu_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t* list = lv_obj_create(ui->_nameMenu);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, us(4), 0);
    lv_obj_set_style_border_width(list, 0, 0);

    fs::FS& fs = Settings.getFS();

    if (fs.exists("/groups.json")) {
        ui->_groupsDoc = std::make_unique<DynamicJsonDocument>(1024);
        File groupsFile = fs.open("/groups.json");
        ReadBufferingStream buffered(groupsFile, 64);
        deserializeJson(*ui->_groupsDoc, buffered);
        groupsFile.close();

        JsonArray groups = ui->_groupsDoc->as<JsonArray>();
        int index = 0;
        for (JsonObjectConst group : groups) {
            lv_obj_t* btn = lv_btn_create(list);
            lv_obj_set_width(btn, LV_PCT(100));
            lv_obj_set_height(btn, us(36));
            lv_obj_set_style_bg_color(btn, tc(TC_SURFACE_RAISED), 0);
            lv_obj_set_style_shadow_width(btn, 0, 0);
            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, group["name"] | "Unnamed Group");
            lv_obj_center(lbl);

            lv_obj_set_user_data(btn, (void*)(uintptr_t)index);
            lv_obj_add_event_cb(btn, group_selected_event_cb, LV_EVENT_CLICKED, ui);
            index++;
        }
    } else {
        lv_obj_t* lbl = lv_label_create(list);
        lv_label_set_text(lbl, "No groups found");
        lv_obj_center(lbl);
    }
}

void LocoUI::group_selected_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    int index = (uintptr_t)lv_obj_get_user_data(btn);

    if (!ui->_groupsDoc) return;
    JsonArray groups = ui->_groupsDoc->as<JsonArray>();
    if (index >= 0 && index < groups.size()) {
        JsonObjectConst group = groups[index];
        JsonArrayConst locos = group["locos"];

        lv_obj_t* list = lv_obj_get_child(ui->_nameMenu, 1);
        lv_obj_clean(list);

        lv_obj_t* title_row = lv_obj_get_child(ui->_nameMenu, 0);
        lv_obj_t* title = lv_obj_get_child(title_row, 0);
        lv_label_set_text_fmt(title, "Group: %s", group["name"] | "Unknown");

        fs::FS& fs2 = Settings.getFS();
        StaticJsonDocument<16> filterDoc;
        filterDoc["name"] = true;
        StaticJsonDocument<64> locoDoc;
        for (uint16_t address : locos) {
            char path[32];
            sprintf(path, "/locos/%d.json", address);

            char nameStr[56];
            const char* locoName = "Missing";
            if (fs2.exists(path)) {
                locoDoc.clear();
                File locoFile = fs2.open(path);
                ReadBufferingStream buffered(locoFile, 64);
                deserializeJson(locoDoc, buffered, DeserializationOption::Filter(filterDoc));
                locoFile.close();
                locoName = locoDoc["name"] | "Unknown";
            }
            snprintf(nameStr, sizeof(nameStr), "#%d - %s", address, locoName);

            lv_obj_t* loco_btn = lv_btn_create(list);
            lv_obj_set_width(loco_btn, LV_PCT(100));
            lv_obj_set_height(loco_btn, us(36));
            lv_obj_set_style_bg_color(loco_btn, tc(TC_SURFACE_RAISED), 0);
            lv_obj_set_style_shadow_width(loco_btn, 0, 0);
            lv_obj_t* lbl = lv_label_create(loco_btn);
            lv_label_set_text(lbl, nameStr);
            lv_obj_center(lbl);

            lv_obj_set_user_data(loco_btn, (void*)(uintptr_t)address);
            lv_obj_add_event_cb(loco_btn, loco_selected_event_cb, LV_EVENT_CLICKED, ui);
        }
    }
}

void LocoUI::release_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_activeConsist) {
        ui->_dccex.setThrottle(ui->_activeConsist, 0, Direction::Forward);
        ui->_dccex.deleteCSConsist(ui->_activeConsist);
        ui->_activeConsist = nullptr;
        Serial.printf("[DCC] Consist '%s' released\n", ui->_consistName.c_str());
        ui->_locos.remove();
        if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
        lv_async_call([](void* user_data) { ((LocoUI*)user_data)->refresh(); }, ui);
        return;
    }
    if (ui->_loco.address != 0) {
        if (ui->_activeLoco) {
            ui->_dccex.setThrottle(ui->_activeLoco, 0,
                ui->_loco.direction ? Direction::Forward : Direction::Reverse);
            char cmd[16];
            snprintf(cmd, sizeof(cmd), "- %d", ui->_loco.address);
            ui->_dccex.sendCommand(cmd);
            delete ui->_activeLoco;
            ui->_activeLoco = nullptr;
        }
        Serial.printf("[DCC] %s (%d) released\n", ui->_locoName.c_str(), ui->_loco.address);
        ui->_locos.remove();

        if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

        lv_async_call([](void* user_data) {
            ((LocoUI*)user_data)->refresh();
        }, ui);
    }
}

void LocoUI::consist_btn_event_cb(lv_event_t* e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);

    // ConsistUI is self-owned; it deletes itself on Close or Drive
    new ConsistUI(ui->_dccex, lv_layer_top(), [ui](CSConsist* consist, const String& name) {
        ui->_activeConsist = consist;
        ui->_activeLoco = nullptr;
        ui->_loco = {};
        ui->_consistName = name;
        ui->_locoName = name;
        CSConsistMember* lead = consist->getFirstMember();
        ui->_loco.address = lead ? lead->address : 0;
        ui->_locos.add(ui->_loco.address);
        Serial.printf("[DCC] Consist '%s' acquired (lead %d)\n", name.c_str(), ui->_loco.address);
        lv_async_call([](void* user_data) { ((LocoUI*)user_data)->refresh(); }, ui);
    });
}

void LocoUI::addr_btn_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (ui->_selectionMenu) lv_obj_add_flag(ui->_selectionMenu, LV_OBJ_FLAG_HIDDEN);
    ui->showKeypad();
}

void LocoUI::kb_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    if (lv_event_get_code(e) == LV_EVENT_READY) {
        const char* txt = lv_textarea_get_text(ui->_textarea);
        if (txt && strlen(txt) > 0) {
            uint16_t addr = atoi(txt);
            if (addr > 0 && addr <= 9999) {
                ui->_locos.add(addr);
                Settings.pushRecentLoco(addr);
                ui->_keyboard = nullptr;
                ui->_textarea = nullptr;
                lv_async_call([](void* user_data) {
                    ((LocoUI*)user_data)->refresh();
                }, ui);
                return;
            }
        }
    }
    ui->hideKeypad();
}

