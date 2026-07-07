// LocoUI speedometer dial — split out of LocoUI.cpp.
// Builds the gauge face / scale / arc / needle and handles the drag-to-set
// speed arc and the custom needle draw. Shared layout constants live in LocoUI.h.
#include "LocoUI.h"
#include "Theme.h"
#include <math.h>

void LocoUI::buildGauge() {
        lv_coord_t faceSize = (lv_coord_t)(lv_display_get_horizontal_resolution(lv_display_get_default()) * 45 / 100);
        _gaugeFaceRadius = faceSize / 2;

        // Vertical offset of the throttle from the container centre.
#if defined(TFT_HEIGHT) && TFT_HEIGHT >= 480
        // 3.5": centre the throttle on the mid-line of the 4 function buttons.
        lv_coord_t contentH = (lv_coord_t)(lv_display_get_vertical_resolution(lv_display_get_default()) - 70);
        int btnH = us(36);
        int fnCentre = (int)(contentH * fnTopOffset() / 250)
                     + (FN_ROWS - 1) * (int)(contentH * fnSpacing() / 250) / 2
                     + btnH / 2;
        int gaugeY = fnCentre - contentH / 2;
#else
        int gaugeY = us(13);
#endif

        // Dark gauge face — plain circle drawn as a sibling BEFORE the scale so
        // it sits behind the transparent scale widget. Ticks and labels on the
        // scale overflow outside this face boundary.
        lv_obj_t* gaugeFace = lv_obj_create(_container);
        lv_obj_set_size(gaugeFace, faceSize, faceSize);
        lv_obj_align(gaugeFace, LV_ALIGN_CENTER, 0, gaugeY);
        lv_obj_set_style_radius(gaugeFace, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(gaugeFace, tc(TC_GAUGE_BG), 0);
        lv_obj_set_style_bg_opa(gaugeFace, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(gaugeFace, 0, 0);
        lv_obj_set_style_shadow_width(gaugeFace, 0, 0);
        lv_obj_clear_flag(gaugeFace, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(gaugeFace, LV_OBJ_FLAG_CLICKABLE);

        // Scale widget — transparent, same size as face. ROUND_OUTER mode puts
        // labels outside the arc ring, which overflows the widget boundary.
        _speedScale = lv_scale_create(_container);
        lv_obj_set_size(_speedScale, faceSize, faceSize);
        lv_scale_set_mode(_speedScale, LV_SCALE_MODE_ROUND_OUTER);
        lv_scale_set_range(_speedScale, 0, 120);   // 0-120 display range; needle maps 0-126 → 0-120
        lv_scale_set_total_tick_count(_speedScale, 25);   // minor every 5 steps, 7 major ticks
        lv_scale_set_major_tick_every(_speedScale, 4);    // majors at 0,20,40,60,80,100,120
        lv_scale_set_angle_range(_speedScale, 270);
        lv_scale_set_rotation(_speedScale, 135);
        lv_obj_align(_speedScale, LV_ALIGN_CENTER, 0, gaugeY);
        lv_obj_clear_flag(_speedScale, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(_speedScale, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

        // Transparent — face bg comes from the sibling gaugeFace behind
        lv_obj_set_style_bg_opa(_speedScale, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(_speedScale, 0, 0);
        lv_obj_set_style_pad_all(_speedScale, 0, 0);

        // Arc ring at face boundary
        lv_obj_set_style_arc_color(_speedScale, tc(TC_GAUGE_RING), LV_PART_MAIN);
        lv_obj_set_style_arc_width(_speedScale, 5, LV_PART_MAIN);

        // Major ticks + labels
        lv_obj_set_style_length(_speedScale, 12, LV_PART_INDICATOR);
        lv_obj_set_style_line_color(_speedScale, tc(TC_GAUGE_TICK_MAJOR), LV_PART_INDICATOR);
        lv_obj_set_style_line_width(_speedScale, 2, LV_PART_INDICATOR);
        lv_obj_set_style_text_color(_speedScale, tc(TC_GAUGE_LABEL), LV_PART_INDICATOR);
        lv_obj_set_style_text_font(_speedScale, &lv_font_montserrat_12, LV_PART_INDICATOR);
        lv_obj_set_style_pad_radial(_speedScale, -3, LV_PART_INDICATOR);

        // Minor ticks
        lv_obj_set_style_length(_speedScale, 8, LV_PART_ITEMS);
        lv_obj_set_style_line_color(_speedScale, tc(TC_GAUGE_TICK_MINOR), LV_PART_ITEMS);
        lv_obj_set_style_line_width(_speedScale, 1, LV_PART_ITEMS);

        // Tapered needle — drawn as a filled triangle in DRAW_MAIN_END (before children)
        lv_obj_add_event_cb(_speedScale, gauge_needle_draw_cb, LV_EVENT_DRAW_MAIN_END, this);

        // Transparent arc on top — captures drag/touch for speed control
        _speedArc = lv_arc_create(_container);
        lv_obj_set_size(_speedArc, faceSize, faceSize);
        lv_arc_set_rotation(_speedArc, 135);
        lv_arc_set_bg_angles(_speedArc, 0, 270);
        lv_arc_set_range(_speedArc, 0, 126);
        lv_arc_set_value(_speedArc, _loco.speed);
        lv_obj_set_style_opa(_speedArc, LV_OPA_TRANSP, 0);
        lv_obj_align(_speedArc, LV_ALIGN_CENTER, 0, gaugeY);
        lv_obj_add_event_cb(_speedArc, speed_arc_event_cb, LV_EVENT_VALUE_CHANGED, this);

        // Masking circle — covers needle root so speed number reads cleanly
        lv_coord_t hubSize = faceSize * 42 / 100;
        lv_obj_t* hub = lv_obj_create(_speedScale);
        lv_obj_set_size(hub, hubSize, hubSize);
        lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(hub, tc(TC_GAUGE_BG), 0);
        lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(hub, 0, 0);
        lv_obj_set_style_shadow_color(hub, lv_color_hex(0x000000), 0);
        lv_obj_set_style_shadow_width(hub, 10, 0);
        lv_obj_set_style_shadow_spread(hub, 2, 0);
        lv_obj_set_style_shadow_opa(hub, LV_OPA_80, 0);
        lv_obj_set_style_shadow_offset_y(hub, 2, 0);
        lv_obj_clear_flag(hub, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(hub, LV_OBJ_FLAG_SCROLLABLE);

        // Speed readout — parented to hub so it centres on the visible circle
        _speedLabel = lv_label_create(hub);
        lv_label_set_text_fmt(_speedLabel, "%d", _loco.speed);
        lv_obj_set_style_text_align(_speedLabel, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(_speedLabel, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(_speedLabel, tc(TC_GAUGE_NEEDLE), 0);
        lv_obj_center(_speedLabel);

}

void LocoUI::gauge_needle_draw_cb(lv_event_t* e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* scale = (lv_obj_t*)lv_event_get_target(e);
    lv_layer_t* layer = lv_event_get_layer(e);

    // Angle: rotation=135°, sweep=270°, display range 0-120 mapped from speed 0-126
    float displayVal = (float)ui->_loco.speed * 120.0f / 126.0f;
    float angleRad   = (135.0f + displayVal * 270.0f / 120.0f) * (float)M_PI / 180.0f;

    lv_area_t coords;
    lv_obj_get_coords(scale, &coords);
    float cx        = (coords.x1 + coords.x2) * 0.5f;
    float cy        = (coords.y1 + coords.y2) * 0.5f;
    float needleLen = (float)ui->_gaugeFaceRadius * 1.10f;
    float baseHalf  = 4.5f;

    float dx = cosf(angleRad), dy = sinf(angleRad); // along needle
    float px = -dy,            py =  dx;             // perpendicular

    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.p[0].x = cx + dx * needleLen;  dsc.p[0].y = cy + dy * needleLen; // tip
    dsc.p[1].x = cx + px * baseHalf;   dsc.p[1].y = cy + py * baseHalf;  // base +side
    dsc.p[2].x = cx - px * baseHalf;   dsc.p[2].y = cy - py * baseHalf;  // base −side
    dsc.color = tc(TC_GAUGE_NEEDLE);
    dsc.opa   = LV_OPA_COVER;
    lv_draw_triangle(layer, &dsc);
}

void LocoUI::speed_arc_event_cb(lv_event_t * e) {
    LocoUI* ui = (LocoUI*)lv_event_get_user_data(e);
    lv_obj_t* arc = (lv_obj_t*)lv_event_get_target(e);
    int speed = lv_arc_get_value(arc);

    if (ui->_speedLabel) lv_label_set_text_fmt(ui->_speedLabel, "%d", speed);
    if (ui->_dirBtn) lv_obj_set_style_opa(lv_obj_get_parent(ui->_dirBtn), speed == 0 ? LV_OPA_COVER : LV_OPA_40, 0);
    if (ui->_speedScale) lv_obj_invalidate(ui->_speedScale);

    ui->_loco.speed = speed;
    ui->_lastLocalSpeedMs = millis();
    if (ui->_activeConsist) {
        ui->_dccex.setThrottle(ui->_activeConsist, speed,
            ui->_loco.direction ? Direction::Forward : Direction::Reverse);
    } else if (ui->_activeLoco) {
        ui->_dccex.setThrottle(ui->_activeLoco, speed,
            ui->_loco.direction ? Direction::Forward : Direction::Reverse);
    }
}
