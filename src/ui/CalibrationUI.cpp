#include "CalibrationUI.h"
#include <Settings.h>

CalibrationUI::CalibrationUI() {
    _container = nullptr;
    _instructions = nullptr;
    _target = nullptr;
    _pollTimer = nullptr;
    _state = 0;
}

CalibrationUI::~CalibrationUI() {
    hide();
}

void CalibrationUI::show() {
    if (_container) return;

    _state = 0;

    // Create full screen overlay
    _container = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_container, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(_container, 0, 0);
    lv_obj_set_style_border_width(_container, 0, 0);
    lv_obj_set_style_bg_color(_container, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(_container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(_container, 0, 0);

    _instructions = lv_label_create(_container);
    lv_obj_set_style_text_color(_instructions, lv_color_make(255, 255, 255), 0);
    lv_obj_set_width(_instructions, LV_PCT(90));
    lv_obj_set_style_text_align(_instructions, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(_instructions, "Touch Calibration\n\nTap the target in the Top Left.");
    lv_obj_center(_instructions);

    _target = lv_obj_create(_container);
    lv_obj_set_size(_target, 40, 40);
    lv_obj_set_style_bg_color(_target, lv_color_make(255, 0, 0), 0);
    lv_obj_set_style_radius(_target, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(_target, 2, 0);
    lv_obj_set_style_border_color(_target, lv_color_make(255, 255, 255), 0);

    drawTarget(30, 30);

    // Create high frequency timer to poll the raw touch driver directly
    _pollTimer = lv_timer_create(poll_timer_cb, 50, this);
}

void CalibrationUI::hide() {
    if (_pollTimer) {
        lv_timer_del(_pollTimer);
        _pollTimer = nullptr;
    }
    if (_container) {
        lv_obj_del(_container);
        _container = nullptr;
    }
}

void CalibrationUI::drawTarget(int x, int y) {
    lv_obj_set_pos(_target, x - 20, y - 20);
}

void CalibrationUI::poll_timer_cb(lv_timer_t* timer) {
    CalibrationUI* ui = (CalibrationUI*)lv_timer_get_user_data(timer);
    if (ui) ui->processTouch();
}

void CalibrationUI::processTouch() {
    bool isTouched = touchscreen.isTouched();

    if (_state == 0) { // Wait for Top Left Press
        if (isTouched) {
            Point p = touchscreen.getTouch();
            _rx1 = p.x;
            _ry1 = p.y;
            _state = 1;
            lv_label_set_text(_instructions, "Release...");
            lv_obj_set_style_bg_color(_target, lv_color_make(0, 255, 0), 0);
        }
    } else if (_state == 1) { // Wait for Release
        if (!isTouched) {
            _state = 2;
            lv_label_set_text(_instructions, "Touch Calibration\n\nTap the target in the Bottom Right.");
            lv_obj_set_style_bg_color(_target, lv_color_make(255, 0, 0), 0);
            drawTarget(lv_obj_get_width(_container) - 30, lv_obj_get_height(_container) - 30);
        }
    } else if (_state == 2) { // Wait for Bottom Right Press
        if (isTouched) {
            Point p = touchscreen.getTouch();
            _rx2 = p.x;
            _ry2 = p.y;
            _state = 3;
            lv_label_set_text(_instructions, "Release...");
            lv_obj_set_style_bg_color(_target, lv_color_make(0, 255, 0), 0);
        }
    } else if (_state == 3) { // Wait for Final Release
        if (!isTouched) {
            // Extrapolate bounds based on universal orientation mapping.
            // 1. Get current display rotation
            lv_disp_t * display = lv_disp_get_default();
            lv_display_rotation_t rotation = lv_display_get_rotation(display);

            // 2. Define UI coordinates of the button centers
            int ui_x1 = 30, ui_y1 = 30;
            int ui_x2 = lv_obj_get_width(_container) - 30;
            int ui_y2 = lv_obj_get_height(_container) - 30;

            // 3. Map UI coordinates to Physical touch coordinates (0-239, 0-319)
            // LVGL 9 rotates touch input internally. We reverse that rotation to find 
            // the expected physical mapping for the buttons.
            int x_map1, y_map1, x_map2, y_map2;
            
            if (rotation == LV_DISPLAY_ROTATION_0) {
                x_map1 = ui_x1; y_map1 = ui_y1;
                x_map2 = ui_x2; y_map2 = ui_y2;
            } else if (rotation == LV_DISPLAY_ROTATION_90) {
                x_map1 = 240 - ui_y1; y_map1 = ui_x1;
                x_map2 = 240 - ui_y2; y_map2 = ui_x2;
            } else if (rotation == LV_DISPLAY_ROTATION_180) {
                x_map1 = 240 - ui_x1; y_map1 = 320 - ui_y1;
                x_map2 = 240 - ui_x2; y_map2 = 320 - ui_y2;
            } else { // LV_DISPLAY_ROTATION_270
                x_map1 = ui_y1; y_map1 = 320 - ui_x1;
                x_map2 = ui_y2; y_map2 = 320 - ui_x2;
            }

            // 4. Calculate xMin/xMax
            // main.cpp maps p.x to 239..0 using: map(p.x, xMin, xMax, 239, 0)
            // This means xMin corresponds to 239, and xMax corresponds to 0.
            float mx = (float)(_rx2 - _rx1) / (x_map2 - x_map1);
            float cx = _rx1 - mx * x_map1;
            int xMin = round(mx * 239 + cx);
            int xMax = round(mx * 0 + cx);

            // 5. Calculate yMin/yMax
            // main.cpp maps p.y to 0..319 using: map(p.y, yMin, yMax, 0, 319)
            // This means yMin corresponds to 0, and yMax corresponds to 319.
            float my = (float)(_ry2 - _ry1) / (y_map2 - y_map1);
            float cy = _ry1 - my * y_map1;
            int yMin = round(my * 0 + cy);
            int yMax = round(my * 319 + cy);

            // Safety Check: Validate the calculated bounds before saving.
            // A typical 12-bit ADC ranges from 0 to 4095. 
            // We allow extrapolation margins up to +/- 2000.
            // The difference between min and max should also be substantial (at least 1000).
            bool isValid = true;
            if (abs(xMax - xMin) < 1000 || abs(yMax - yMin) < 1000) isValid = false;
            if (xMin < -2000 || xMin > 6000 || xMax < -2000 || xMax > 6000) isValid = false;
            if (yMin < -2000 || yMin > 6000 || yMax < -2000 || yMax > 6000) isValid = false;

            Serial.println("[LCD] Touch calibration complete");
            Serial.printf("[LCD] Point 1 (Top Left): Raw X=%d, Y=%d\n", _rx1, _ry1);
            Serial.printf("[LCD] Point 2 (Bottom Right): Raw X=%d, Y=%d\n", _rx2, _ry2);
            Serial.printf("[LCD] Old Bounds - xMin: %d, xMax: %d, yMin: %d, yMax: %d\n",
                Settings.TouchCal.xMin, Settings.TouchCal.xMax, Settings.TouchCal.yMin, Settings.TouchCal.yMax);
            Serial.printf("[LCD] New Bounds - xMin: %d, xMax: %d, yMin: %d, yMax: %d\n",
                xMin, xMax, yMin, yMax);
            Serial.printf("[LCD] Adjustments - dxMin: %d, dxMax: %d, dyMin: %d, dyMax: %d\n",
                xMin - Settings.TouchCal.xMin,
                xMax - Settings.TouchCal.xMax,
                yMin - Settings.TouchCal.yMin,
                yMax - Settings.TouchCal.yMax);

            if (isValid) {
                Settings.TouchCal.xMin = xMin;
                Settings.TouchCal.xMax = xMax;
                Settings.TouchCal.yMin = yMin;
                Settings.TouchCal.yMax = yMax;
                Settings.save();
            } else {
                Serial.println("[LCD] WARNING: Calibration rejected due to extreme values!");
            }

            hide();
        }
    }
}
