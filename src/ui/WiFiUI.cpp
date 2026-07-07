#include "WiFiUI.h"
#include "Theme.h"

static const int KB_HEIGHT = 165;

WiFiUI::WiFiUI(lv_obj_t* parent) {
    _keyboard = nullptr;

    _container = lv_obj_create(parent);
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_pad_all(_container, us(5), 0);
    lv_obj_set_style_pad_hor(_container, us(11), 0);
    lv_obj_set_style_pad_row(_container, us(3), 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_flex_flow(_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    _ipGotHandler = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (_labelIP) lv_label_set_text_fmt(_labelIP, "IP: %s", WiFi.localIP().toString().c_str());
    }, ARDUINO_EVENT_WIFI_STA_GOT_IP);

    _ipDisconnectedHandler = WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (_labelIP) lv_label_set_text(_labelIP, "IP: Not connected");
    }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // --- Title row ---
    lv_obj_t* title_row = lv_obj_create(_container);
    lv_obj_set_width(title_row, LV_PCT(100));
    lv_obj_set_height(title_row, us(36));
    lv_obj_set_style_pad_all(title_row, 0, 0);
    lv_obj_set_style_border_width(title_row, 0, 0);
    lv_obj_set_style_bg_opa(title_row, 0, 0);
    lv_obj_clear_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* title = lv_label_create(title_row);
    lv_label_set_text(title, "WiFi & CS settings");
    lv_obj_set_style_text_color(title, tc(TC_SECTION), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* back_btn = make_danger_btn(title_row, "Back");
    lv_obj_set_size(back_btn, LV_SIZE_CONTENT, us(28));
    lv_obj_set_style_pad_hor(back_btn, us(10), 0);
    lv_obj_align(back_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(back_btn, close_btn_event_cb, LV_EVENT_CLICKED, this);

    auto add_section = [this](const char* title) {
        lv_obj_t* lbl = lv_label_create(_container);
        lv_label_set_text(lbl, title);
        lv_obj_set_style_text_color(lbl, tc(TC_SECTION), 0);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_obj_set_style_pad_top(lbl, us(6), 0);
    };

    auto add_label = [this](const char* text) {
        lv_obj_t* lbl = lv_label_create(_container);
        lv_label_set_text(lbl, text);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_obj_set_style_text_color(lbl, tc(TC_TEXT_HINT), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_pad_top(lbl, us(2), 0);
    };

    auto create_textarea = [this](const char* placeholder, const char* initial_text, int field) -> lv_obj_t* {
        lv_obj_t* ta = lv_textarea_create(_container);
        lv_obj_set_width(ta, LV_PCT(100));
        lv_textarea_set_one_line(ta, true);
        lv_textarea_set_placeholder_text(ta, placeholder);
        lv_textarea_set_text(ta, initial_text);
        lv_obj_set_user_data(ta, (void*)(uintptr_t)field);
        lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_ALL, this);
        return ta;
    };

    // --- Network ---
    add_section("Network");

    add_label("SSID");
    _textareaSSID = create_textarea("Network name", Settings.CS.SSID().c_str(), 0);

    add_label("Password");
    _textareaPassword = create_textarea("Password", Settings.CS.password().c_str(), 1);
    lv_textarea_set_password_mode(_textareaPassword, true);

    // --- Command Station ---
    add_section("Command station");

    add_label("Server address");
    _textareaServer = create_textarea("IP or hostname.local", Settings.CS.server().c_str(), 2);

    add_label("Port");
    char portBuf[8];
    snprintf(portBuf, sizeof(portBuf), "%d", Settings.CS.port());
    _textareaPort = create_textarea("Port", portBuf, 3);

    _labelIP = lv_label_create(_container);
    if (WiFi.isConnected())
        lv_label_set_text_fmt(_labelIP, "IP: %s", WiFi.localIP().toString().c_str());
    else
        lv_label_set_text(_labelIP, "IP: Not connected");
    lv_obj_set_style_text_color(_labelIP, tc(TC_TEXT_MUTED), 0);
    lv_obj_set_style_text_font(_labelIP, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_top(_labelIP, us(4), 0);

    // --- Access Point ---
    add_section("Access point");

    lv_obj_t* ap_lbl = lv_label_create(_container);
    lv_label_set_text_fmt(ap_lbl, "SSID: %s\nPassword: %s", Settings.AP.SSID.c_str(), Settings.AP.password.c_str());
    lv_obj_set_style_text_color(ap_lbl, tc(TC_TEXT_HINT), 0);
    lv_obj_set_style_text_font(ap_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_pad_top(ap_lbl, us(2), 0);

}

WiFiUI::~WiFiUI() {
    WiFi.removeEvent(_ipGotHandler);
    WiFi.removeEvent(_ipDisconnectedHandler);
    if (_keyboard) {
        lv_keyboard_set_textarea(_keyboard, NULL);
        lv_obj_del(_keyboard);
    }
    if (_container) lv_obj_del(_container);
}

void WiFiUI::ta_event_cb(lv_event_t* e) {
    WiFiUI* ui = (WiFiUI*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    int field = (int)(uintptr_t)lv_obj_get_user_data(ta);

    if (code == LV_EVENT_FOCUSED) {
        if (!ui->_keyboard) {
            ui->_keyboard = lv_keyboard_create(lv_layer_top());
            lv_obj_add_event_cb(ui->_keyboard, kb_event_cb, LV_EVENT_ALL, ui);
        }
        lv_keyboard_set_textarea(ui->_keyboard, ta);
        lv_keyboard_set_mode(ui->_keyboard, (field == 3) ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_obj_clear_flag(ui->_keyboard, LV_OBJ_FLAG_HIDDEN);

        lv_obj_set_style_pad_bottom(ui->_container, KB_HEIGHT, 0);
        lv_obj_scroll_to_view(ta, LV_ANIM_ON);

    } else if (code == LV_EVENT_DEFOCUSED) {
        if (ui->_keyboard) {
            lv_keyboard_set_textarea(ui->_keyboard, NULL);
            lv_obj_add_flag(ui->_keyboard, LV_OBJ_FLAG_HIDDEN);
        }
        lv_obj_set_style_pad_bottom(ui->_container, us(5), 0);

        const char* txt = lv_textarea_get_text(ta);
        if (field == 0) Settings.CS.SSID(txt);
        else if (field == 1) Settings.CS.password(txt);
        else if (field == 2) Settings.CS.server(txt);
        else if (field == 3) Settings.CS.port(atoi(txt));

        if (Settings.CS.valid()) {
            Settings.save();
            Settings.dispatchEvent(SettingsClass::Event::CS_CHANGE, reinterpret_cast<void*>(1));
        }
    }
}

void WiFiUI::kb_event_cb(lv_event_t* e) {
    WiFiUI* ui = (WiFiUI*)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (ui->_keyboard) {
            lv_obj_t* ta = lv_keyboard_get_textarea(ui->_keyboard);
            if (ta) lv_obj_clear_state(ta, LV_STATE_FOCUSED);
        }
    }
}

void WiFiUI::close_btn_event_cb(lv_event_t* e) {
    WiFiUI* ui = (WiFiUI*)lv_event_get_user_data(e);
    lv_obj_add_flag(ui->_container, LV_OBJ_FLAG_HIDDEN);
}
