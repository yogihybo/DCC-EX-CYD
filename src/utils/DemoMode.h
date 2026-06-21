#pragma once
#include <LVGL_CYD.h>

// =============================================================================
// Demo Mode — captures a screenshot sequence across all UI tabs.
//
// HOW TO RE-ENABLE:
//
//   1. LocoUI.h      — uncomment:  // void demoStep(int step);
//   2. LocoUI.cpp    — uncomment:  // void LocoUI::demoStep(int step) { ... }
//   3. PowerUI.h     — uncomment:  // void demoStep(int step);
//   4. PowerUI.cpp   — uncomment:  // void PowerUI::demoStep(int step) { ... }
//   5. SettingsUI.cpp — uncomment: // #include "../utils/DemoMode.h"
//   6. SettingsUI.cpp — uncomment the demo button row in the constructor:
//        // b = make_row(DemoMode::trigger); make_name(b, "Demo mode");
//        // make_badge(b, "TEMP", ...); make_chevron(b);
//
// This file and DemoMode.cpp require no changes — they compile as-is.
// =============================================================================

namespace DemoMode {
    // Use directly as an LVGL event callback on the settings button row
    void trigger(lv_event_t* e);
}
