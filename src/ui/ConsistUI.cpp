#include "ConsistUI.h"
#include "Theme.h"
#include <Settings.h>

LV_FONT_DECLARE(fa_gauge_high_16);

// ── helpers ──────────────────────────────────────────────────────────────────

String ConsistUI::_consistPath(int leadAddr) {
    char buf[32];
    snprintf(buf, sizeof(buf), "/consists/%d.json", leadAddr);
    return String(buf);
}

// Looks up a loco name from the roster, returns empty string if not found.
static String locoName(int address) {
    char path[32];
    snprintf(path, sizeof(path), "/locos/%d.json", address);
    fs::FS& fs = Settings.getFS();
    if (fs.exists(path)) {
        fs::File f = fs.open(path);
        StaticJsonDocument<128> doc;
        if (deserializeJson(doc, f) == DeserializationError::Ok && doc.containsKey("name"))
            return doc["name"].as<const char*>();
        f.close();
    }
    return String();
}

// ── constructor / destructor ──────────────────────────────────────────────────

ConsistUI::ConsistUI(DCCEXProtocol& dccex, lv_obj_t* parent, DriveCallback onDrive)
    : _dccex(dccex), _onDrive(std::move(onDrive)) {

    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(_container, 0, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_style_bg_opa(_container, LV_OPA_COVER, 0);
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);

    // Header — fixed height; flex gives the rest to _content
    lv_obj_t* header = lv_obj_create(_container);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, us(50));
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_grow(header, 0);

    lv_obj_t* title = lv_label_create(header);
    lv_label_set_text(title, "Consists");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, us(10), 0);

    lv_obj_t* close_btn = make_danger_btn(header, "Close");
    lv_obj_align(close_btn, LV_ALIGN_RIGHT_MID, us(-10), 0);
    lv_obj_add_event_cb(close_btn, close_btn_cb, LV_EVENT_CLICKED, this);

    _showList();
}

ConsistUI::~ConsistUI() {
    if (_container) lv_obj_del(_container);
}

// ── state renderers ───────────────────────────────────────────────────────────

void ConsistUI::_clearContent() {
    if (_content) { lv_obj_del(_content); _content = nullptr; }
    _keyboard = nullptr;
    _addrTextarea = nullptr;
}

void ConsistUI::_showList() {
    _clearContent();
    _editLeadAddr = -1;
    _members.clear();
    _editName = "";

    _content = lv_obj_create(_container);
    lv_obj_set_width(_content, LV_PCT(100));
    lv_obj_set_flex_grow(_content, 1);
    lv_obj_set_style_pad_all(_content, us(5), 0);
    lv_obj_set_style_border_width(_content, 0, 0);
    lv_obj_set_flex_flow(_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_content, us(4), 0);

    // New consist button
    lv_obj_t* new_btn = lv_btn_create(_content);
    lv_obj_set_width(new_btn, LV_PCT(100));
    lv_obj_set_height(new_btn, us(38));
    lv_obj_set_style_bg_color(new_btn, lv_color_make(40, 140, 40), 0);
    lv_obj_set_style_shadow_width(new_btn, 0, 0);
    lv_obj_t* new_lbl = lv_label_create(new_btn);
    lv_label_set_text(new_lbl, "+ New Consist");
    lv_obj_center(new_lbl);
    lv_obj_add_event_cb(new_btn, new_consist_cb, LV_EVENT_CLICKED, this);

    // Saved consists
    fs::FS& fs = Settings.getFS();
    if (!fs.exists("/consists")) fs.mkdir("/consists");
    fs::File dir = fs.open("/consists");
    if (dir && dir.isDirectory()) {
        while (fs::File f = dir.openNextFile()) {
            if (!f.isDirectory() && String(f.name()).endsWith(".json")) {
                StaticJsonDocument<512> doc;
                if (deserializeJson(doc, f) == DeserializationError::Ok) {
                    String name = doc["name"] | String(f.name());
                    int memberCount = doc["members"].size();

                    lv_obj_t* row = lv_obj_create(_content);
                    lv_obj_set_width(row, LV_PCT(100));
                    lv_obj_set_height(row, us(38));
                    lv_obj_set_style_pad_ver(row, us(4), 0);
                    lv_obj_set_style_pad_hor(row, us(2), 0);
                    lv_obj_set_style_pad_column(row, us(4), 0);
                    lv_obj_set_style_border_width(row, 0, 0);
                    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
                    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

                    lv_obj_t* lbl = lv_label_create(row);
                    lv_label_set_text_fmt(lbl, "%s  (%d)", name.c_str(), memberCount);
                    lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
                    lv_obj_set_flex_grow(lbl, 1);

                    // Store lead address from first member
                    int leadAddr = doc["members"][0]["address"] | -1;

                    // Throttle button (drive)
                    lv_obj_t* drv_btn = lv_btn_create(row);
                    lv_obj_set_size(drv_btn, us(34), us(30));
                    lv_obj_set_style_shadow_width(drv_btn, 0, 0);
                    lv_obj_set_style_bg_color(drv_btn, lv_color_make(40, 140, 40), 0);
                    lv_obj_t* drv_lbl = lv_label_create(drv_btn);
                    lv_label_set_text(drv_lbl, "\xEF\x98\xA5");  // loco/throttle icon (U+F625, fa_gauge_high_16)
                    lv_obj_set_style_text_font(drv_lbl, &fa_gauge_high_16, 0);
                    lv_obj_center(drv_lbl);
                    lv_obj_set_user_data(drv_btn, (void*)(intptr_t)leadAddr);
                    lv_obj_add_event_cb(drv_btn, consist_drive_cb, LV_EVENT_CLICKED, this);

                    // Settings button (edit)
                    lv_obj_t* set_btn = lv_btn_create(row);
                    lv_obj_set_size(set_btn, us(34), us(30));
                    lv_obj_set_style_shadow_width(set_btn, 0, 0);
                    lv_obj_t* set_lbl = lv_label_create(set_btn);
                    lv_label_set_text(set_lbl, LV_SYMBOL_SETTINGS);
                    lv_obj_center(set_lbl);
                    lv_obj_set_user_data(set_btn, (void*)(intptr_t)leadAddr);
                    lv_obj_add_event_cb(set_btn, consist_item_cb, LV_EVENT_CLICKED, this);
                }
                f.close();
            }
        }
        dir.close();
    }
}

void ConsistUI::_rebuildMemberRows(lv_obj_t* list) {
    // Delete all children tagged as member rows
    uint32_t childCount = lv_obj_get_child_count(list);
    for (int i = (int)childCount - 1; i >= 0; i--) {
        lv_obj_t* child = lv_obj_get_child(list, i);
        if (lv_obj_get_user_data(child) != nullptr)
            lv_obj_del(child);
    }

    for (int i = 0; i < (int)_members.size(); i++) {
        const MemberData& m = _members[i];
        String name = locoName(m.address);

        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_width(row, LV_PCT(100));
        lv_obj_set_height(row, us(38));
        lv_obj_set_style_pad_all(row, us(3), 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_user_data(row, (void*)(intptr_t)(i + 1)); // non-null tag

        // Address + name
        char rank[4];
        if (i == 0) rank[0] = 'L', rank[1] = '\0';
        else snprintf(rank, sizeof(rank), "%d", i + 1);
        lv_obj_t* lbl = lv_label_create(row);
        if (name.isEmpty())
            lv_label_set_text_fmt(lbl, "[%s] %d", rank, m.address);
        else
            lv_label_set_text_fmt(lbl, "[%s] %d %s", rank, m.address, name.c_str());
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(lbl, us(110));

        // Direction toggle
        lv_obj_t* dir_btn = lv_btn_create(row);
        lv_obj_set_size(dir_btn, us(44), us(28));
        lv_obj_set_style_shadow_width(dir_btn, 0, 0);
        lv_obj_align(dir_btn, LV_ALIGN_RIGHT_MID, us(-48), 0);
        lv_obj_set_style_bg_color(dir_btn,
            m.reversed ? lv_color_make(180, 120, 30) : lv_color_make(40, 140, 40), 0);
        lv_obj_t* dir_lbl = lv_label_create(dir_btn);
        lv_label_set_text(dir_lbl, m.reversed ? "REV" : "FWD");
        lv_obj_set_style_text_font(dir_lbl, &lv_font_montserrat_10, 0);
        lv_obj_center(dir_lbl);
        lv_obj_set_user_data(dir_btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(dir_btn, toggle_reversed_cb, LV_EVENT_CLICKED, this);

        // Remove button (not for lead loco)
        lv_obj_t* rem_btn = make_danger_btn(row, LV_SYMBOL_TRASH);
        lv_obj_set_size(rem_btn, us(38), us(28));
        lv_obj_align(rem_btn, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_user_data(rem_btn, (void*)(intptr_t)i);
        lv_obj_add_event_cb(rem_btn, remove_member_cb, LV_EVENT_CLICKED, this);
        if (i == 0) lv_obj_add_state(rem_btn, LV_STATE_DISABLED); // lead can't be removed
    }
}

void ConsistUI::_showEditor() {
    _clearContent();

    _content = lv_obj_create(_container);
    lv_obj_set_width(_content, LV_PCT(100));
    lv_obj_set_flex_grow(_content, 1);
    lv_obj_set_style_pad_all(_content, us(5), 0);
    lv_obj_set_style_border_width(_content, 0, 0);
    lv_obj_set_flex_flow(_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_content, us(3), 0);

    // Name field
    lv_obj_t* name_row = lv_obj_create(_content);
    lv_obj_set_width(name_row, LV_PCT(100));
    lv_obj_set_height(name_row, us(36));
    lv_obj_set_style_pad_all(name_row, us(3), 0);
    lv_obj_set_style_border_width(name_row, 0, 0);
    lv_obj_clear_flag(name_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(name_row, nullptr);

    lv_obj_t* name_lbl = lv_label_create(name_row);
    lv_label_set_text(name_lbl, "Name:");
    lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* name_ta = lv_textarea_create(name_row);
    lv_obj_set_size(name_ta, us(150), us(30));
    lv_obj_align(name_ta, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_textarea_set_one_line(name_ta, true);
    lv_textarea_set_text(name_ta, _editName.isEmpty() ? "New Consist" : _editName.c_str());
    lv_obj_add_event_cb(name_ta, name_edit_cb, LV_EVENT_VALUE_CHANGED, this);

    // Replicate functions toggle
    lv_obj_t* rep_row = lv_obj_create(_content);
    lv_obj_set_width(rep_row, LV_PCT(100));
    lv_obj_set_height(rep_row, us(32));
    lv_obj_set_style_pad_all(rep_row, us(3), 0);
    lv_obj_set_style_border_width(rep_row, 0, 0);
    lv_obj_clear_flag(rep_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(rep_row, nullptr);

    lv_obj_t* rep_lbl = lv_label_create(rep_row);
    lv_label_set_text(rep_lbl, "Replicate Fns:");
    lv_obj_set_style_text_font(rep_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(rep_lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* rep_sw = lv_switch_create(rep_row);
    lv_obj_align(rep_sw, LV_ALIGN_RIGHT_MID, 0, 0);
    if (_replicateFunctions) lv_obj_add_state(rep_sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb(rep_sw, replicate_fn_cb, LV_EVENT_VALUE_CHANGED, this);

    // Member list (scrollable) — grows to fill remaining flex space
    lv_obj_t* member_list = lv_obj_create(_content);
    lv_obj_set_width(member_list, LV_PCT(100));
    lv_obj_set_flex_grow(member_list, 1);
    lv_obj_set_style_pad_all(member_list, us(2), 0);
    lv_obj_set_style_pad_row(member_list, us(2), 0);
    lv_obj_set_style_border_width(member_list, 1, 0);
    lv_obj_set_flex_flow(member_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_user_data(member_list, nullptr);

    _rebuildMemberRows(member_list);

    // Add member button
    lv_obj_t* add_btn = lv_btn_create(_content);
    lv_obj_set_width(add_btn, LV_PCT(100));
    lv_obj_set_height(add_btn, us(35));
    lv_obj_set_style_shadow_width(add_btn, 0, 0);
    lv_obj_t* add_lbl = lv_label_create(add_btn);
    lv_label_set_text(add_lbl, "+ Add Loco");
    lv_obj_center(add_lbl);
    lv_obj_set_user_data(add_btn, (void*)member_list);
    lv_obj_add_event_cb(add_btn, add_member_btn_cb, LV_EVENT_CLICKED, this);

    // Action row: Drive / Save / Delete
    lv_obj_t* act_row = lv_obj_create(_content);
    lv_obj_set_width(act_row, LV_PCT(100));
    lv_obj_set_height(act_row, us(38));
    lv_obj_set_style_pad_all(act_row, 0, 0);
    lv_obj_set_style_border_width(act_row, 0, 0);
    lv_obj_set_flex_flow(act_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(act_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(act_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_user_data(act_row, nullptr);

    lv_obj_t* drive_btn = lv_btn_create(act_row);
    lv_obj_set_flex_grow(drive_btn, 1);
    lv_obj_set_height(drive_btn, us(35));
    lv_obj_set_style_shadow_width(drive_btn, 0, 0);
    lv_obj_set_style_bg_color(drive_btn, lv_color_make(40, 140, 40), 0);
    lv_obj_t* drive_lbl = lv_label_create(drive_btn);
    lv_label_set_text(drive_lbl, LV_SYMBOL_PLAY " Drive");
    lv_obj_center(drive_lbl);
    lv_obj_add_event_cb(drive_btn, drive_btn_cb, LV_EVENT_CLICKED, this);
    if (_members.size() < 2) lv_obj_add_state(drive_btn, LV_STATE_DISABLED);

    lv_obj_t* save_btn = lv_btn_create(act_row);
    lv_obj_set_flex_grow(save_btn, 1);
    lv_obj_set_height(save_btn, us(35));
    lv_obj_set_style_shadow_width(save_btn, 0, 0);
    lv_obj_t* save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, LV_SYMBOL_SAVE " Save");
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(save_btn, save_btn_cb, LV_EVENT_CLICKED, this);
    if (_members.empty()) lv_obj_add_state(save_btn, LV_STATE_DISABLED);

    if (_editLeadAddr != -1) {
        lv_obj_t* del_btn = make_danger_btn(act_row, LV_SYMBOL_TRASH);
        lv_obj_set_flex_grow(del_btn, 1);
        lv_obj_set_height(del_btn, us(35));
        lv_obj_add_event_cb(del_btn, delete_btn_cb, LV_EVENT_CLICKED, this);
    }
}

void ConsistUI::_showAddMember() {
    _clearContent();

    _content = lv_obj_create(_container);
    lv_obj_set_width(_content, LV_PCT(100));
    lv_obj_set_flex_grow(_content, 1);
    lv_obj_set_style_pad_all(_content, us(5), 0);
    lv_obj_set_style_border_width(_content, 0, 0);
    lv_obj_set_flex_flow(_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_content, us(4), 0);

    lv_obj_t* prompt = lv_label_create(_content);
    lv_label_set_text(prompt, _members.empty() ? "Lead loco address:" : "Member loco address:");

    _addrTextarea = lv_textarea_create(_content);
    lv_obj_set_width(_addrTextarea, LV_PCT(100));
    lv_textarea_set_one_line(_addrTextarea, true);
    lv_textarea_set_accepted_chars(_addrTextarea, "0123456789");
    lv_textarea_set_max_length(_addrTextarea, 4);
    lv_textarea_set_placeholder_text(_addrTextarea, "DCC address");

    // Direction toggle (not shown for lead loco)
    if (!_members.empty()) {
        lv_obj_t* dir_row = lv_obj_create(_content);
        lv_obj_set_width(dir_row, LV_PCT(100));
        lv_obj_set_height(dir_row, us(34));
        lv_obj_set_style_pad_all(dir_row, us(3), 0);
        lv_obj_set_style_border_width(dir_row, 0, 0);
        lv_obj_clear_flag(dir_row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* dir_lbl = lv_label_create(dir_row);
        lv_label_set_text(dir_lbl, "Reversed:");
        lv_obj_align(dir_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t* dir_sw = lv_switch_create(dir_row);
        lv_obj_align(dir_sw, LV_ALIGN_RIGHT_MID, 0, 0);
        _addMemberReversed = false;
        lv_obj_add_event_cb(dir_sw, [](lv_event_t* e) {
            ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
            ui->_addMemberReversed = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED);
        }, LV_EVENT_VALUE_CHANGED, this);
    }

    _keyboard = lv_keyboard_create(_content);
    lv_keyboard_set_mode(_keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_keyboard_set_textarea(_keyboard, _addrTextarea);
    lv_obj_add_event_cb(_keyboard, keyboard_cb, LV_EVENT_READY, this);
    lv_obj_add_event_cb(_keyboard, keyboard_cb, LV_EVENT_CANCEL, this);
}

// ── persistence ───────────────────────────────────────────────────────────────

void ConsistUI::_saveConsist() {
    if (_members.empty()) return;
    fs::FS& fs = Settings.getFS();
    if (!fs.exists("/consists")) fs.mkdir("/consists");

    StaticJsonDocument<512> doc;
    doc["name"] = _editName.isEmpty() ? "Consist" : _editName;
    doc["replicateFunctions"] = _replicateFunctions;
    JsonArray arr = doc.createNestedArray("members");
    for (const MemberData& m : _members) {
        JsonObject obj = arr.createNestedObject();
        obj["address"]  = m.address;
        obj["reversed"] = m.reversed;
    }

    fs::File f = fs.open(_consistPath(_members[0].address), "w", true);
    if (f) { serializeJson(doc, f); f.close(); }
}

void ConsistUI::_deleteConsistFile() {
    if (_editLeadAddr == -1) return;
    Settings.getFS().remove(_consistPath(_editLeadAddr));
}

// ── event callbacks ───────────────────────────────────────────────────────────

void ConsistUI::close_btn_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    delete ui;
}

void ConsistUI::new_consist_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    ui->_members.clear();
    ui->_editName = "";
    ui->_editLeadAddr = -1;
    ui->_replicateFunctions = false;
    ui->_showAddMember(); // Start by picking the lead loco
}

void ConsistUI::consist_drive_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    int leadAddr = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));

    String path = _consistPath(leadAddr);
    fs::File f = Settings.getFS().open(path);
    if (!f) return;

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    f.close();

    String name = doc["name"] | String("Consist");
    bool repFn   = doc["replicateFunctions"] | false;
    JsonArray members = doc["members"].as<JsonArray>();
    if (members.size() < 2) return;

    CSConsist* consist = ui->_dccex.createCSConsist(
        members[0]["address"].as<int>(), members[0]["reversed"] | false, repFn);
    if (!consist) return;

    for (size_t i = 1; i < members.size(); i++)
        ui->_dccex.addCSConsistMember(consist, members[i]["address"].as<int>(), members[i]["reversed"] | false);

    if (ui->_onDrive) ui->_onDrive(consist, name);
    delete ui;
}

void ConsistUI::consist_item_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    int leadAddr = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));

    String path = _consistPath(leadAddr);
    fs::File f = Settings.getFS().open(path);
    if (!f) return;

    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    f.close();

    ui->_editName = doc["name"] | String("Consist");
    ui->_replicateFunctions = doc["replicateFunctions"] | false;
    ui->_editLeadAddr = leadAddr;
    ui->_members.clear();
    for (JsonObject m : doc["members"].as<JsonArray>()) {
        ui->_members.push_back({ m["address"].as<int>(), m["reversed"] | false });
    }
    ui->_showEditor();
}

void ConsistUI::add_member_btn_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    ui->_showAddMember();
}

void ConsistUI::keyboard_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY) {
        if (ui->_addrTextarea) {
            int addr = atoi(lv_textarea_get_text(ui->_addrTextarea));
            if (addr > 0 && addr <= 10239) {
                ui->_members.push_back({ addr, ui->_addMemberReversed });
                // Update lead addr if this is the first member
                if (ui->_members.size() == 1) ui->_editLeadAddr = -1; // will be set on save
            }
        }
    }
    ui->_showEditor();
}

void ConsistUI::remove_member_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
    if (idx > 0 && idx < (int)ui->_members.size())
        ui->_members.erase(ui->_members.begin() + idx);
    ui->_showEditor();
}

void ConsistUI::toggle_reversed_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
    if (idx >= 0 && idx < (int)ui->_members.size())
        ui->_members[idx].reversed = !ui->_members[idx].reversed;
    ui->_showEditor();
}

void ConsistUI::replicate_fn_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    ui->_replicateFunctions = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED);
}

void ConsistUI::name_edit_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    ui->_editName = lv_textarea_get_text((lv_obj_t*)lv_event_get_target(e));
}

void ConsistUI::drive_btn_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    if (ui->_members.size() < 2) return;

    // Save before driving
    ui->_saveConsist();

    // Create CSConsist via DCCEXProtocol
    CSConsist* consist = ui->_dccex.createCSConsist(
        ui->_members[0].address, ui->_members[0].reversed, ui->_replicateFunctions);
    if (!consist) return;

    for (int i = 1; i < (int)ui->_members.size(); i++)
        ui->_dccex.addCSConsistMember(consist, ui->_members[i].address, ui->_members[i].reversed);

    String name = ui->_editName;
    if (ui->_onDrive) ui->_onDrive(consist, name);

    delete ui; // close overlay
}

void ConsistUI::save_btn_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    ui->_saveConsist();
    ui->_showList();
}

void ConsistUI::delete_btn_cb(lv_event_t* e) {
    ConsistUI* ui = (ConsistUI*)lv_event_get_user_data(e);
    ui->_deleteConsistFile();
    ui->_showList();
}
