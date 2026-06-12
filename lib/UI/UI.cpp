#include <Touch.h>
#include <UI.h>

void UI::loop() {}

void UI::handleRedraw() {
  auto redraw = [](Component *ui) {
    for (const auto &ptr : ui->_elements) {
      ptr->draw();
    }
    ui->redraw();
  };

  if (_child != nullptr) {
    _child->handleRedraw();
  } else {
    redraw(this);
    for (const auto &component : _components) {
      redraw(component.get());
    }
  }
}

bool UI::handleTouch(uint8_t count, TouchPoint *points,
                     std::function<bool()> touched) {
  auto touchElements = [&points](auto &elements, auto method) -> bool {
    for (const auto &ptr : elements) {
      if (ptr->contains(points[0].x, points[0].y)) {
        std::invoke(method, ptr, ptr.get());
        return true;
      }
    }
    return false;
  };

  auto handleTouchMethod = [this, &touchElements](auto method) -> bool {
    if (touchElements(_elements, method)) {
      return true;
    }
    for (const auto &component : _components) {
      if (touchElements(component->_elements, method)) {
        return true;
      }
    }
    return false;
  };

  if (_child != nullptr) {
    return _child->handleTouch(count, points, touched);
  } else if (touch(count, points) || handleTouchMethod(&Touch::touched)) {
    while (touched()) {
      delay(25);
    }
    return release(count, points) || handleTouchMethod(&Touch::released);
  }

  return false;
}

void UI::handleEncoderRotate(Encoder::Rotation rotation) {
  if (_child != nullptr) {
    _child->handleEncoderRotate(rotation);
  } else {
    if (encoderRotate(rotation)) {
      return;
    }
    for (const auto &component : _components) {
      component->encoderRotate(rotation);
    }
  }
}

void UI::handleEncoderPress(Encoder::ButtonPress press) {
  if (_child != nullptr) {
    _child->handleEncoderPress(press);
  } else {
    if (encoderPress(press)) {
      return;
    }
    for (const auto &component : _components) {
      component->encoderPress(press);
    }
  }
}

void UI::handleSwipe(Swipe swipe) {
  if (_child != nullptr) {
    _child->handleSwipe(swipe);
  } else {
    if (this->swipe(swipe)) {
      return;
    }
    for (const auto &component : _components) {
      component->swipe(swipe);
    }
  }
}

void UI::handleTasks() {
  if (_child != nullptr) {
    _child->handleTasks();
  }

  while (!_tasks.empty()) {
    _tasks[0]();
    _tasks.pop_front();
  }
}

void UI::handleLoop() {
  if (_child != nullptr) {
    _child->handleLoop();
  }

  loop();
}

void UI::reset(bool redraw) {
#if defined(CYD_ESP32)
  UI::tft->fillRect(0, (30 * TFT_HEIGHT) / 480, TFT_WIDTH,
                    TFT_HEIGHT - (30 * TFT_HEIGHT) / 480, UI::COLOR_MAIN_BG);
#else
  UI::tft->fillRect(0, 30, 320, 450, UI::COLOR_MAIN_BG);
#endif
  _child.reset();
  if (redraw) {
    handleRedraw();
  }
}

// void UI::setBacklight(uint8_t percentage) {
// #if defined(CYD_ESP32)
//   if (percentage > 100)
//     percentage = 100;
//  Active-low: 0% brightness = 255 duty cycle, 100% brightness = 0 duty cycle
// uint8_t duty = 255 - (percentage * 255) / 100;
//  analogWrite(TFT_BL, duty);
// #endif
//}
