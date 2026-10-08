#pragma once

#include "HardwareProfile.h"
#include "TouchDriver.h"

namespace hw {

struct TouchPoint {
    int16_t x;
    int16_t y;
};

// The single place where raw touch readings become screen coordinates:
//   raw --(TouchCalibration)--> native panel pixel --(rotation)--> screen pixel
// The rotation step uses the convention documented in DisplayDriver.h.
class TouchMapper {
public:
    TouchMapper(const TouchCalibration& cal, uint16_t nativeWidth, uint16_t nativeHeight);

    TouchPoint toNative(const RawTouch& raw) const;
    TouchPoint toScreen(const RawTouch& raw, uint8_t rotation) const;

    void setCalibration(const TouchCalibration& cal) { _cal = cal; }
    const TouchCalibration& calibration() const { return _cal; }

private:
    TouchCalibration _cal;
    uint16_t _w;
    uint16_t _h;
};

}  // namespace hw
