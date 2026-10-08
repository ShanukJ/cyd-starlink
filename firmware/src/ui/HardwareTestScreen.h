#pragma once

#include <lvgl.h>

#include "../hardware/Board.h"
#include "../network/WifiStatus.h"
#include "../starlink/StarlinkService.h"
#include "LvglPort.h"

namespace ui {

// Milestone 1 bring-up screen. Shows the *measured* state of each hardware
// component, live touch coordinates with a crosshair, colour bars to check
// RGB order / inversion, a rotation toggle and a brightness slider.
class HardwareTestScreen {
public:
    HardwareTestScreen(hw::Board& board, LvglPort& port) : _board(board), _port(port) {}

    using Callback = void (*)(void* ctx);
    void build(Callback onBack, void* ctx);
    lv_obj_t* screen() const { return _screen; }
    bool built() const { return _screen != nullptr; }
    void setWifiStatus(const net::WifiStatus& s);
    void setStarlink(const starlink::StarlinkSnapshot& s);

private:
    enum class Mark { Ok, Warn, Fail, Pending, Busy, NotImplemented };

    lv_obj_t* addRow(lv_obj_t* parent, const char* name);
    void setRow(lv_obj_t* value, Mark mark, const char* text);
    void refreshDisplayRow();
    void update();

    static void onTimer(lv_timer_t* timer);
    static void onRotate(lv_event_t* e);
    static void onBrightness(lv_event_t* e);
    static void onBackClicked(lv_event_t* e);
    void onDeleted();

    hw::Board& _board;
    LvglPort& _port;

    Callback _onBack = nullptr;
    void* _ctx = nullptr;
    lv_obj_t* _screen = nullptr;
    lv_obj_t* _displayValue = nullptr;
    lv_obj_t* _wifiValue = nullptr;
    lv_obj_t* _starlinkValue = nullptr;
    lv_obj_t* _touchValue = nullptr;
    lv_obj_t* _coords = nullptr;
    lv_obj_t* _raw = nullptr;
    lv_obj_t* _crosshair = nullptr;  // on the top layer: not deleted with the screen
    lv_timer_t* _timer = nullptr;

    uint32_t _shownPressCount = UINT32_MAX;
    bool _shownPressed = false;
    hw::TouchPoint _shownPoint{-1, -1};
    hw::RawTouch _shownRaw{UINT16_MAX, UINT16_MAX, 0};
    int _shownIrq = -2;
    Mark _shownWifiMark = Mark::NotImplemented;
    char _shownWifiText[24] = "--";
    Mark _shownStarlinkMark = Mark::NotImplemented;
    char _shownStarlinkText[24] = "--";
};

}  // namespace ui
