#ifndef BOTTOM_BAR_H
#define BOTTOM_BAR_H

#include <UI.h>
#include <Elements/TabButton.h>

class BottomBar : public UI, public Events {
  public:
    enum class Tab : uint8_t {
      LOCO = 0,
      ACCESSORIES = 1,
      POWER = 2,
      SETTINGS = 3
    };

    struct Event {
      static constexpr uint8_t NAVIGATE = 0;
    };
  private:
    Tab _activeTab;
    TabButton* _buttons[4];

    void selectTab(Tab tab, bool fireEvent = true);
  public:
    BottomBar();
    virtual ~BottomBar() = default;

    void redraw() override;
    Tab getActiveTab() const;
    void setActiveTab(Tab tab);
};

#endif
