#ifndef ACCESSORIES_UI_H
#define ACCESSORIES_UI_H

#include <UI.h>
#include <DCCExCS.h>

class AccessoriesUI : public UI {
  private:
    DCCExCS& _dccExCS;

    void showKeypad(bool state);
  public:
    AccessoriesUI(DCCExCS& dccExCS);
    virtual ~AccessoriesUI() = default;
};

#endif
