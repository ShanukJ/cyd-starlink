#include "Backlight.h"

#include <Arduino.h>

namespace hw {

bool Backlight::begin() {
    if (_cfg.pin < 0) return false;
    _ok = ledcAttach(_cfg.pin, _cfg.pwmHz, _cfg.pwmBits);
    if (!_ok) {
        // Fall back to plain on/off so the screen is at least visible.
        pinMode(_cfg.pin, OUTPUT);
    }
    set(0);
    return _ok;
}

void Backlight::set(uint8_t level) {
    _level = level;
    if (_cfg.pin < 0) return;
    if (!_ok) {
        digitalWrite(_cfg.pin, (level > 0) == _cfg.activeHigh ? HIGH : LOW);
        return;
    }
    const uint32_t maxDuty = (1u << _cfg.pwmBits) - 1;
    uint32_t duty = static_cast<uint32_t>(level) * maxDuty / 255;
    if (!_cfg.activeHigh) duty = maxDuty - duty;
    ledcWrite(_cfg.pin, duty);
}

}  // namespace hw
