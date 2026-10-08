#pragma once

#include "HardwareProfile.h"

namespace hw {

// PWM backlight on an LEDC channel. Brightness is 0..255 regardless of the
// PWM resolution configured in the profile.
class Backlight {
public:
    explicit Backlight(const BacklightConfig& cfg) : _cfg(cfg) {}

    bool begin();  // starts with the backlight off
    void set(uint8_t level);
    uint8_t level() const { return _level; }
    bool dimmable() const { return _ok; }

private:
    const BacklightConfig& _cfg;
    bool _ok = false;
    uint8_t _level = 0;
};

}  // namespace hw
