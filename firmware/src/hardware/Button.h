#pragma once

#include <stdint.h>

#include "HardwareProfile.h"

namespace hw {

// Polled push button that reports a single event when held for holdMs.
class Button {
public:
    Button(const ButtonConfig& cfg, uint32_t holdMs) : _cfg(cfg), _holdMs(holdMs) {}

    void begin();
    // Call regularly; returns true once per hold, when the hold time is reached.
    bool pollHeld(uint32_t nowMs);

private:
    const ButtonConfig& _cfg;
    uint32_t _holdMs;
    uint32_t _downSince = 0;
    bool _down = false;
    bool _fired = false;
};

}  // namespace hw
