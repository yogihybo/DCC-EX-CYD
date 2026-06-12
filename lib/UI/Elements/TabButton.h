#ifndef TAB_BUTTON_H
#define TAB_BUTTON_H

#include <Element.h>

class TabButton : public Element {
  public:
    enum class Icon : uint8_t {
      LOCO,
      ACCESSORIES,
      POWER,
      SETTINGS
    };
  private:
    Icon _icon;
    bool _active;
  public:
    TabButton(int16_t x, int16_t y, uint16_t w, uint16_t h, Icon icon, bool active = false);
    virtual ~TabButton() = default;

    inline Type getType() { return Type::TabButton; }
    void draw();

    void setActive(bool active, bool redraw = true);
    bool isActive() const;
    Icon getIcon() const;
};

#endif
