#pragma once

#include <LVGL_CYD.h>
#include <DCCExCS.h>
#include "LVGL_Layouts.h"

class ProgramUI : public UIView {
  public:
    enum class Step : uint8_t {
      WRITE_ADDRESS_GET_ADDRESS,

      READ_CV_BYTE_GET_CV,
      WRITE_CV_BYTE_GET_CV,
      WRITE_CV_BYTE_GET_VALUE,

      READ_CV_BIT_GET_CV,
      READ_CV_BIT_GET_BIT,
      WRITE_CV_BIT_GET_CV,
      WRITE_CV_BIT_GET_BIT,
      WRITE_CV_BIT_GET_VALUE,
      
      ACK_LIMIT,
      ACK_MIN,
      ACK_MAX
    };
  private:
    lv_obj_t* _container;
    lv_obj_t* _msgbox;
    lv_obj_t* _keyboard;
    lv_obj_t* _ta; // Text area for numeric input

    DCCExCS& _dccExCS;
    uint16_t _timeoutHandler;
    uint16_t _writeHandler;
    uint16_t _readHandler;
    Step _step;
    uint16_t _stepData[3];

    void newStep(Step step, const String& title, uint16_t max, uint16_t min);
    void keypadEnter(uint32_t number);
    void confirm(const String& message);
    void working();
    void result(const String& message, lv_color_t color);
    void clearMsgBox();

    static void close_btn_event_cb(lv_event_t * e);
    static void menu_btn_event_cb(lv_event_t * e);
    static void msgbox_close_cb(lv_event_t * e);
    static void msgbox_delete_cb(lv_event_t * e);
    static void keypad_event_cb(lv_event_t * e);
    static void confirm_btn_event_cb(lv_event_t * e);

  public:
    ProgramUI(DCCExCS& dccExCS, lv_obj_t* parent);
    ~ProgramUI() override;
};
