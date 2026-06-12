#ifndef SETTINGS_UI_H
#define SETTINGS_UI_H

#include <UI.h>
#include <Settings.h>

class SettingsUI : public UI, public Events {
  private:
    class Page {
      protected:
        SettingsUI& _ui;
      public:
        Page(SettingsUI& ui) : _ui(ui) { }

        virtual void show() = 0;
    };

    class Page1 : Page {
      using Page::Page;

      char _speedStepLabels[3][2] = { "1", "2", "4" };
      char _swipActionLabels[7][12] = { "None", "Keypad", "Loco Names", "Loco Groups", "Next Loco", "Prev Loco", "Release" };

      void swipeAction(void* button, uint8_t& gesture, uint8_t actions);
    public:
      void show();
    } Page1;

    class Page2 : Page {
      using Page::Page;

      char _rotationLabels[3][30] = { "Standard", "Inverted", "Accelerometer (if applicable)" };
      char _pinLabels[2][8] = { "Not Set", "Pin Set" };
    public:
      void show();
    } Page2;

    class Page3 : Page {
      using Page::Page;
    public:
      void show();
    } Page3;

    std::vector<std::function<void (void)>> _pages = {
      [this] { Page1.show(); },
      [this] { Page2.show(); },
      [this] { Page3.show(); }
    };
  public:
    struct Event {
      static constexpr uint8_t WIFI = 0;
      static constexpr uint8_t ABOUT = 1;
    };

    SettingsUI();
    ~SettingsUI();
};

#endif
