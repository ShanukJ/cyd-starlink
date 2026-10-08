#pragma once

#include <stdint.h>

namespace hw {

// Controller-native touch reading, before calibration or rotation. For a
// resistive controller these are ADC counts; a capacitive controller would
// typically report panel pixels (identity calibration).
struct RawTouch {
    uint16_t x;
    uint16_t y;
    uint16_t pressure;
};

// Abstract touch controller. Calibration and rotation are NOT done here —
// see TouchMapper — so every controller is handled the same way.
class TouchDriver {
public:
    virtual ~TouchDriver() = default;

    virtual bool begin() = 0;
    virtual const char* controllerName() const = 0;

    // Returns true and fills `out` while the panel is being touched.
    virtual bool read(RawTouch& out) = 0;
};

}  // namespace hw
