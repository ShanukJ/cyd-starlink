#pragma once

#include <memory>

#include "Backlight.h"
#include "DisplayDriver.h"
#include "HardwareProfile.h"
#include "TouchDriver.h"
#include "TouchMapper.h"

namespace hw {

// Results of hardware bring-up, as measured — not assumed.
struct BoardStatus {
    bool displayOk = false;
    bool displayReadable = false;  // panel answered register reads
    bool displayAwake = false;     // RDDPM reports booster on, awake, display on
    PanelReadback readback{};
    bool touchOk = false;
    bool backlightDimmable = false;
};

// Owns the drivers for one HardwareProfile. Everything above this layer
// uses the abstract DisplayDriver / TouchDriver / Backlight interfaces.
class Board {
public:
    explicit Board(const HardwareProfile& profile);

    // Initialises display, touch and backlight. The backlight is left off;
    // turn it on once the first frame has been drawn.
    const BoardStatus& begin();

    const HardwareProfile& profile() const { return _profile; }
    const BoardStatus& status() const { return _status; }
    DisplayDriver& display() { return *_display; }
    TouchDriver* touch() { return _touchOk ? _touch.get() : nullptr; }
    TouchMapper& touchMapper() { return _touchMapper; }
    Backlight& backlight() { return _backlight; }

private:
    const HardwareProfile& _profile;
    std::unique_ptr<DisplayDriver> _display;
    std::unique_ptr<TouchDriver> _touch;
    bool _touchOk = false;
    TouchMapper _touchMapper;
    Backlight _backlight;
    BoardStatus _status;
};

}  // namespace hw
