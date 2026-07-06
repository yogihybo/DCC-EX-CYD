#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <Settings.h>
#ifdef ST7796_DRIVER
#  include "LVGL_CYD.h"
#else
#  include "XPT2046_Bitbang.h"
   extern XPT2046_Bitbang touchscreen;
#endif

class CalibrationUI {
public:
    CalibrationUI();
    ~CalibrationUI();

    void show();
    void hide();

private:
    lv_obj_t* _container;
    lv_obj_t* _instructions;
    lv_obj_t* _target;
    lv_timer_t* _pollTimer;

    int _state; // 0 = Wait for Pt1, 1 = Wait for Release 1, 2 = Wait for Pt2, 3 = Wait for Release 2
    int _rx1, _ry1;
    int _rx2, _ry2;

    static void poll_timer_cb(lv_timer_t* timer);
    void processTouch();
    void drawTarget(int x, int y);
};
