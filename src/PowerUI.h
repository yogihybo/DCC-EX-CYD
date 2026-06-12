#ifndef POWER_UI_H
#define POWER_UI_H

#include <UI.h>
#include <DCCExCS.h>
#include <Elements/Button.h>

class PowerUI : public UI {
  private:
    DCCExCS& _dccExCS;
    DCCExCS::Power& _power;
    uint8_t _broadcastPowerHandler;

    Button* _powerAll;
    Button* _powerMain;
    Button* _powerProg;
    Button* _powerJoin;
  public:
    PowerUI(DCCExCS& dccExCS, DCCExCS::Power& power);
    virtual ~PowerUI();
};

#endif
