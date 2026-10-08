#include "TouchMapper.h"

#include <utility>

namespace hw {

namespace {

int16_t scale(int32_t raw, int32_t rawMin, int32_t rawMax, int32_t size) {
    if (rawMax == rawMin) return 0;
    int32_t v = (raw - rawMin) * (size - 1) / (rawMax - rawMin);
    if (v < 0) return 0;
    if (v > size - 1) return size - 1;
    return v;
}

}  // namespace

TouchMapper::TouchMapper(const TouchCalibration& cal, uint16_t nativeWidth, uint16_t nativeHeight)
    : _cal(cal), _w(nativeWidth), _h(nativeHeight) {}

TouchPoint TouchMapper::toNative(const RawTouch& raw) const {
    int32_t rx = raw.x;
    int32_t ry = raw.y;
    if (_cal.swapXY) std::swap(rx, ry);
    return {scale(rx, _cal.rawXMin, _cal.rawXMax, _w), scale(ry, _cal.rawYMin, _cal.rawYMax, _h)};
}

TouchPoint TouchMapper::toScreen(const RawTouch& raw, uint8_t rotation) const {
    const TouchPoint n = toNative(raw);
    switch (rotation & 3) {
        case 1: return {n.y, static_cast<int16_t>(_w - 1 - n.x)};
        case 2: return {static_cast<int16_t>(_w - 1 - n.x), static_cast<int16_t>(_h - 1 - n.y)};
        case 3: return {static_cast<int16_t>(_h - 1 - n.y), n.x};
        default: return n;
    }
}

}  // namespace hw
