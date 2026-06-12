#include <AccessoriesUI.h>
#include <Elements/Header.h>
#include <Elements/Button.h>
#include <Children/Keypad.h>

AccessoriesUI::AccessoriesUI(DCCExCS& dccExCS) : _dccExCS(dccExCS) {
  _elements.reserve(10);

  addElement<Header>(0, 40, 320, 18, "Accessory / Turnout Control");

  // Modern Cards for ON and OFF triggers
  addElement<Button>(10, 90, 300, 70, "Turnout ON", "Turnout ON", false)
    ->onRelease([this](void*) {
      showKeypad(true);
    });

  addElement<Button>(10, 180, 300, 70, "Turnout OFF", "Turnout OFF", false)
    ->onRelease([this](void*) {
      showKeypad(false);
    });
}

void AccessoriesUI::showKeypad(bool state) {
  auto keypad = setChild<Keypad>("Accessory Address", 2044, 1);
  keypad->addEventListener(Keypad::Event::ENTER, [this, state](void* parameter) {
    uint16_t address = *static_cast<uint16_t*>(parameter);
    _dccExCS.accessory(address, state);
    reset(true);
  });
  keypad->addEventListener(Keypad::Event::CANCEL, [this](void*) {
    reset(true);
  });
}
