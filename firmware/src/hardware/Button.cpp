#include "Button.h"

#include <Arduino.h>

namespace hw {

void Button::begin() {
    if (_cfg.pin >= 0) pinMode(_cfg.pin, _cfg.activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
}

bool Button::pollHeld(uint32_t nowMs) {
    if (_cfg.pin < 0) return false;
    const bool down = (digitalRead(_cfg.pin) == LOW) == _cfg.activeLow;
    if (!down) {
        _down = false;
        _fired = false;
        return false;
    }
    if (!_down) {
        _down = true;
        _downSince = nowMs;
    }
    if (!_fired && nowMs - _downSince >= _holdMs) {
        _fired = true;
        return true;
    }
    return false;
}

}  // namespace hw
