#include "LvglPort.h"

#include <Arduino.h>
#include <esp_heap_caps.h>

#include "../utils/Log.h"

namespace ui {

namespace {

// Partial render buffers: 30 lines of the longest side, two of them so LVGL
// renders the next band while DMA sends the previous one (~19 KB each).
constexpr size_t kBufferLines = 30;

uint32_t tickMs() { return millis(); }

void logCb(lv_log_level_t, const char* msg) { Serial.printf("[LVGL] %s", msg); }

}  // namespace

bool LvglPort::begin(hw::Board& board) {
    _board = &board;
    auto& display = board.display();

    lv_init();
    lv_tick_set_cb(tickMs);
    lv_log_register_print_cb(logCb);

    const uint16_t longSide = max(display.nativeWidth(), display.nativeHeight());
    const size_t bufBytes = longSide * kBufferLines * sizeof(uint16_t);
    void* buf1 = heap_caps_malloc(bufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void* buf2 = heap_caps_malloc(bufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!buf1 || !buf2) {
        LOG("LVGL", "ERROR: cannot allocate %u B draw buffers", (unsigned)bufBytes * 2);
        return false;
    }

    _disp = lv_display_create(display.width(), display.height());
    lv_display_set_user_data(_disp, this);
    lv_display_set_color_format(_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(_disp, buf1, buf2, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(_disp, flushCb);

    _indev = lv_indev_create();
    lv_indev_set_type(_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(_indev, this);
    lv_indev_set_display(_indev, _disp);
    lv_indev_set_read_cb(_indev, readTouchCb);
    // Long-press opens service screens; make it deliberate.
    lv_indev_set_long_press_time(_indev, 1500);

    LOG("LVGL", "v%d.%d.%d initialized, 2 x %u B draw buffers", lv_version_major(), lv_version_minor(),
        lv_version_patch(), (unsigned)bufBytes);
    return true;
}

void LvglPort::setRotation(uint8_t rotation) {
    auto& display = _board->display();
    display.setRotation(rotation);
    lv_display_set_resolution(_disp, display.width(), display.height());
    // LVGL keeps reading the last point while released; a point from the
    // previous orientation can be outside the new resolution.
    _touch.point = {0, 0};
    LOG("DISPLAY", "Rotation %u (%ux%u)", display.rotation(), display.width(), display.height());
}

void LvglPort::flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
    auto* self = static_cast<LvglPort*>(lv_display_get_user_data(disp));
    auto& display = self->_board->display();
    const int32_t w = lv_area_get_width(area);
    const int32_t h = lv_area_get_height(area);

    // The previous transfer used the *other* buffer; wait for it before
    // queueing this one. Signalling ready right away is safe because LVGL
    // alternates buffers, and the next flush waits again before reuse.
    display.waitIdle();
    if (display.wantsSwappedRgb565()) lv_draw_sw_rgb565_swap(pxMap, w * h);
    display.writePixels(area->x1, area->y1, w, h, reinterpret_cast<const uint16_t*>(pxMap));
    lv_display_flush_ready(disp);
}

void LvglPort::readTouchCb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* self = static_cast<LvglPort*>(lv_indev_get_user_data(indev));
    auto& board = *self->_board;
    auto& t = self->_touch;

    hw::RawTouch raw;
    hw::TouchDriver* touch = board.touch();
    const bool pressed = touch && touch->read(raw);

    if (pressed) {
        t.raw = raw;
        t.point = board.touchMapper().toScreen(raw, board.display().rotation());
        if (!t.pressed) {
            t.pressCount++;
            LOG("TOUCH", "down raw=(%u,%u) z=%u -> (%d,%d)", raw.x, raw.y, raw.pressure, t.point.x, t.point.y);
        }
    } else if (t.pressed) {
        LOG("TOUCH", "up   raw=(%u,%u) -> (%d,%d)", t.raw.x, t.raw.y, t.point.x, t.point.y);
    }
    t.pressed = pressed;

    data->point.x = t.point.x;
    data->point.y = t.point.y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

}  // namespace ui
