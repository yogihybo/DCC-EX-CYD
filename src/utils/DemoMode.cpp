#include "DemoMode.h"
#include "Screenshot.h"
#include "../AppUI.h"
#include "../ui/LVGL_Layouts.h"
#include <SD.h>

// ---------------------------------------------------------------------------
// Step table
// ---------------------------------------------------------------------------

struct DemoStep {
    uint8_t tab;
    int8_t  locoStep;   // -1 = skip
    int8_t  powerStep;  // -1 = skip
};

static const DemoStep STEPS[] = {
    {0,  0, -1},  // loco: no loco selected
    {0,  1, -1},  // loco: addr 3 "Steam Loco", speed 0 FWD
    {0,  2, -1},  // loco: speed 42 (green arc)
    {0,  3, -1},  // loco: speed 126 (red arc, full throttle)
    {0,  4, -1},  // loco: e-stop, speed 0
    {0,  5, -1},  // loco: direction reversed
    {1, -1, -1},  // accessories tab
    {2, -1,  0},  // power: all off
    {2, -1,  1},  // power: main track on
    {2, -1,  2},  // power: both tracks on
    {3, -1, -1},  // settings tab
};
static const int TOTAL = sizeof(STEPS) / sizeof(STEPS[0]);

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------

static struct {
    int          step  = 0;
    lv_timer_t*  timer = nullptr;
    lv_obj_t*    msgbox = nullptr;
} s;

// ---------------------------------------------------------------------------
// Helpers — shared msgbox styling (mirrors SettingsUI internals)
// ---------------------------------------------------------------------------

static void style_box(lv_obj_t* mbox, const char* title, lv_color_t title_color) {
    lv_obj_set_width(mbox, LV_PCT(88));
    lv_obj_set_style_bg_color(mbox, lv_color_hex(0x1e1e1e), 0);
    lv_obj_set_style_border_color(mbox, lv_color_hex(0x383838), 0);
    lv_obj_set_style_border_width(mbox, 1, 0);
    lv_obj_t* t = lv_msgbox_add_title(mbox, title);
    lv_obj_set_style_text_color(t, title_color, 0);
}

static void style_text(lv_obj_t* txt) {
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(txt, lv_color_hex(0xaaaaaa), 0);
}

// ---------------------------------------------------------------------------
// Step application & timer
// ---------------------------------------------------------------------------

static void apply(int step) {
    const DemoStep& ds = STEPS[step];
    lv_tabview_set_act(main_tabview, ds.tab, LV_ANIM_OFF);
#ifdef DEMO_UI_STEPS_ENABLED
    if (ds.locoStep  >= 0 && locoUI) locoUI->demoStep(ds.locoStep);
    if (ds.powerStep >= 0 && pwrUI)  pwrUI->demoStep(ds.powerStep);
#endif
}

static void timer_cb(lv_timer_t*) {
    char path[32];
    snprintf(path, sizeof(path), "/demo_%02d.bmp", s.step);
    saveScreenshot(path);
    Serial.printf("[Demo] Frame %d/%d  %s\n", s.step + 1, TOTAL, path);

    s.step++;

    if (s.step < TOTAL) {
        apply(s.step);
        return;
    }

    // Finished — clean up and show result
    lv_timer_del(s.timer);
    s.timer = nullptr;
    s.step  = 0;

#ifdef DEMO_UI_STEPS_ENABLED
    if (locoUI) locoUI->demoStep(0);
    if (pwrUI)  pwrUI->demoStep(0);
#endif
    lv_tabview_set_act(main_tabview, 0, LV_ANIM_OFF);

    char summary[72];
    snprintf(summary, sizeof(summary),
             "%d frames saved to SD card:\ndemo_00.bmp – demo_%02d.bmp", TOTAL, TOTAL - 1);

    lv_obj_t* mbox = lv_msgbox_create(lv_layer_top());
    style_box(mbox, "Demo complete", lv_color_make(38, 166, 154));
    style_text(lv_msgbox_add_text(mbox, summary));
    lv_obj_t* ok = lv_msgbox_add_footer_button(mbox, "OK");
    lv_obj_set_style_bg_color(ok, lv_color_hex(0x2e2e2e), 0);
    lv_obj_add_event_cb(ok, [](lv_event_t* e) {
        lv_msgbox_close((lv_obj_t*)lv_event_get_user_data(e));
    }, LV_EVENT_CLICKED, mbox);
    lv_obj_center(mbox);
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

void DemoMode::trigger(lv_event_t*) {
    if (s.msgbox) return;

    s.msgbox = lv_msgbox_create(lv_layer_top());
    style_box(s.msgbox, "Demo mode", lv_color_make(200, 150, 50));
    style_text(lv_msgbox_add_text(s.msgbox,
        "Cycles through all tabs with UI\ninteractions, capturing a screenshot\n"
        "at each step to SD card.\n\nRequires SD card inserted."));

    lv_obj_t* start = lv_msgbox_add_footer_button(s.msgbox, "Start");
    lv_obj_set_style_bg_color(start, lv_color_make(38, 120, 100), 0);
    lv_obj_add_event_cb(start, [](lv_event_t* e) {
        lv_msgbox_close(s.msgbox);
        s.msgbox = nullptr;
        s.step = 0;
        apply(0);
        s.timer = lv_timer_create(timer_cb, 2000, nullptr);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* cancel = lv_msgbox_add_footer_button(s.msgbox, "Cancel");
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0x2e2e2e), 0);
    lv_obj_add_event_cb(cancel, [](lv_event_t* e) {
        lv_msgbox_close(s.msgbox);
        s.msgbox = nullptr;
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_center(s.msgbox);
}
