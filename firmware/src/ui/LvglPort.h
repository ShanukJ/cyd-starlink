#pragma once

#include <lvgl.h>

#include "../hardware/Board.h"

namespace ui {

struct TouchState {
    bool pressed = false;
    hw::RawTouch raw{};       // last raw reading while pressed
    hw::TouchPoint point{};   // last screen coordinate while pressed
    uint32_t pressCount = 0;  // number of distinct presses since boot
};

// Glue between LVGL and the hardware layer: display flushing (DMA, double
// buffered), touch input via TouchMapper, tick source and logging.
class LvglPort {
public:
    bool begin(hw::Board& board);

    // Call from the UI task. Returns ms until LVGL next needs servicing.
    uint32_t handle() { return lv_timer_handler(); }

    void setRotation(uint8_t rotation);
    uint8_t rotation() const { return _board->display().rotation(); }

    const TouchState& touch() const { return _touch; }

private:
    static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap);
    static void readTouchCb(lv_indev_t* indev, lv_indev_data_t* data);

    hw::Board* _board = nullptr;
    lv_display_t* _disp = nullptr;
    lv_indev_t* _indev = nullptr;
    TouchState _touch;
};

}  // namespace ui
