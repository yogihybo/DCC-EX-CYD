#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TouchPoint.h>
#include <Component.h>
#include <Events.h>

#include <vector>
#include <deque>

class UI : public Component {
  public:
    static inline TFT_eSPI* tft;
    static inline uint32_t lastUIChangeMillis = 0;
    static void setBacklight(uint8_t percentage);

    // Theme Colors (RGB565 matching reference image)
    static constexpr uint16_t COLOR_MAIN_BG = 0x18E3;       // `#1b1e24`
    static constexpr uint16_t COLOR_CARD_BG = 0x2987;       // `#2c313c`
    static constexpr uint16_t COLOR_TEXT_MUTED = 0x8C94;    // `#8c92a2`
    static constexpr uint16_t COLOR_CYAN = 0x367A;          // `#34ccd3`
    static constexpr uint16_t COLOR_CYAN_BG = 0x0C4F;       // `#0c2727`
    static constexpr uint16_t COLOR_BORDER = 0x422A;        // `#404550`
    static constexpr uint16_t COLOR_GREEN = 0x2E66;         // `#2ecc71`
  protected:
    std::deque<std::function<void (void)>> _tasks;
    std::unique_ptr<UI> _child;
    std::vector<std::unique_ptr<Component>> _components;

    template <class T, typename... Args>
    inline T* setChild(Args&&... args) {
      reset();
      _child = std::make_unique<T>(args...);
      return static_cast<T*>(_child.get());
    }

    template <class T, typename... Args>
    inline T* addComponent(Args&&... args) {
      return static_cast<T*>(_components.emplace_back(std::make_unique<T>(args...)).get());
    }

    virtual void loop(); 
  public:
    virtual ~UI() = default;

    void handleRedraw();
    bool handleTouch(uint8_t count, TouchPoint* points, std::function<bool()> touched);
    void handleEncoderRotate(Encoder::Rotation rotation);
    void handleEncoderPress(Encoder::ButtonPress press);
    void handleSwipe(Swipe swipe);
    void handleTasks();
    void handleLoop();

    void reset(bool redraw = false);
};

#endif
