#pragma once

#include <LVGL_CYD.h>
#include <DCCEXProtocol.h>
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
      READ_CV_BIT_PENDING,

      WRITE_CV_BIT_GET_CV,
      WRITE_CV_BIT_GET_BIT,
      WRITE_CV_BIT_GET_VALUE,

      WRITE_CV_BYTE_WRITEBACK,

      ACK_LIMIT,
      ACK_MIN,
      ACK_MAX
    };

  private:
    lv_obj_t* _container;
    lv_obj_t* _msgbox;
    lv_obj_t* _keyboard;
    lv_obj_t* _ta;

    DCCEXProtocol& _dccex;
    lv_timer_t* _timeoutTimer = nullptr;
    Step _step;
    uint16_t _stepData[3];

    void newStep(Step step, const char* title, uint16_t max, uint16_t min);
    void keypadEnter(uint32_t number);
    void confirm(const char* message);
    void working();
    void result(const char* message, lv_color_t color);
    void resultWithWriteBack(int cv, int value);
    void clearMsgBox();
    void startTimeout();
    void cancelTimeout();

    static void close_btn_event_cb(lv_event_t* e);
    static void menu_btn_event_cb(lv_event_t* e);
    static void preset_cv_event_cb(lv_event_t* e);
    static void msgbox_close_cb(lv_event_t* e);
    static void msgbox_delete_cb(lv_event_t* e);
    static void keypad_event_cb(lv_event_t* e);
    static void confirm_btn_event_cb(lv_event_t* e);
    static void write_back_btn_cb(lv_event_t* e);
    static void timeout_timer_cb(lv_timer_t* timer);

  public:
    ProgramUI(DCCEXProtocol& dccex, lv_obj_t* parent);
    ~ProgramUI() override;

    void receivedReadLoco(int address);
    void receivedWriteLoco(int address);
    void receivedReadCV(int cv, int value);
    void receivedWriteCV(int cv, int value);
};
