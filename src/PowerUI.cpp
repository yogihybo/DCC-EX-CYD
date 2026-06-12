#include <PowerUI.h>
#include <Elements/Header.h>

PowerUI::PowerUI(DCCExCS& dccExCS, DCCExCS::Power& power)
    : _dccExCS(dccExCS), _power(power) {
  _elements.reserve(10);

  _broadcastPowerHandler = _dccExCS.addEventListener(DCCExCS::Event::BROADCAST_POWER, [this](void*) {
    _tasks.push_back([this] {
      _powerAll->setState(_power.main && _power.prog ? Button::State::PRESSED : Button::State::IDLE);
      _powerMain->setState(_power.main ? Button::State::PRESSED : Button::State::IDLE);
      _powerProg->setState(_power.prog ? Button::State::PRESSED : Button::State::IDLE);
      _powerJoin->setState(_power.join ? Button::State::PRESSED : Button::State::IDLE);
    });
  });

  addElement<Header>(0, 40, 320, 18, "Track Power Control");

  // Main and Programming Track side-by-side
  _powerMain = addElement<Button>(10, 80, 145, 60, "On Main", "Off Main", true, _power.main ? Button::State::PRESSED : Button::State::IDLE);
  _powerMain->onRelease([this](void*) {
    _dccExCS.setCSPower(DCCExCS::Power::MAIN, _powerMain->getState() == Button::State::PRESSED);
  });

  _powerProg = addElement<Button>(165, 80, 145, 60, "On Prog", "Off Prog", true, _power.prog ? Button::State::PRESSED : Button::State::IDLE);
  _powerProg->onRelease([this](void*) {
    _dccExCS.setCSPower(DCCExCS::Power::PROG, _powerProg->getState() == Button::State::PRESSED);
  });

  // Turn ON/OFF all tracks
  _powerAll = addElement<Button>(10, 160, 300, 60, "On All Tracks", "Off All Tracks", true, _power.main && _power.prog ? Button::State::PRESSED : Button::State::IDLE);
  _powerAll->onRelease([this](void*) {
    _dccExCS.setCSPower(DCCExCS::Power::ALL, _powerAll->getState() == Button::State::PRESSED);
  });

  // Join Main and Programming tracks
  _powerJoin = addElement<Button>(10, 240, 300, 60, "Join Tracks", "Join Tracks", true, _power.join ? Button::State::PRESSED : Button::State::IDLE);
  _powerJoin->onRelease([this](void*) {
    _dccExCS.setCSPower(DCCExCS::Power::JOIN, _powerJoin->getState() == Button::State::PRESSED);
  });
}

PowerUI::~PowerUI() {
  _dccExCS.removeEventListener(DCCExCS::Event::BROADCAST_POWER, _broadcastPowerHandler);
}
