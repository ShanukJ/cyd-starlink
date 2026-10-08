#include "Board.h"

#include "../utils/Log.h"
#include "drivers/LgfxSpiDisplay.h"
#include "drivers/Xpt2046Touch.h"

namespace hw {

namespace {

// RDDPM bits: booster on (D7), sleep out (D4), normal mode (D3), display on (D2).
constexpr uint8_t kPowerModeOnMask = 0x9C;

std::unique_ptr<DisplayDriver> createDisplay(const DisplayConfig& cfg) {
    switch (cfg.controller) {
        case DisplayController::ST7789: return std::unique_ptr<DisplayDriver>(new LgfxSpiDisplay(cfg));
    }
    return nullptr;
}

std::unique_ptr<TouchDriver> createTouch(const TouchConfig& cfg) {
    switch (cfg.controller) {
        case TouchController::XPT2046: return std::unique_ptr<TouchDriver>(new Xpt2046Touch(cfg));
        case TouchController::None: break;
    }
    return nullptr;
}

}  // namespace

Board::Board(const HardwareProfile& profile)
    : _profile(profile),
      _display(createDisplay(profile.display)),
      _touch(createTouch(profile.touch)),
      _touchMapper(profile.touch.calibration, profile.display.nativeWidth, profile.display.nativeHeight),
      _backlight(profile.backlight) {}

const BoardStatus& Board::begin() {
    // Backlight first and off, so the panel's power-on garbage is never seen.
    _status.backlightDimmable = _backlight.begin();

    _status.displayOk = _display && _display->begin();
    if (_status.displayOk) {
        _display->setRotation(_profile.defaultRotation);
        LOG("DISPLAY", "%s initialized (%ux%u, rotation %u)", _display->controllerName(), _display->width(),
            _display->height(), _display->rotation());

        _status.displayReadable = _display->readBack(_status.readback);
        const PanelReadback& rb = _status.readback;
        _status.displayAwake = _status.displayReadable && (rb.powerMode & kPowerModeOnMask) == kPowerModeOnMask;
        if (!_status.displayReadable) {
            LOG("DISPLAY", "No readback path (MISO not wired)");
        } else {
            LOG("DISPLAY", "Module ID %02X %02X %02X, power mode 0x%02X (%s), pixel format 0x%02X", rb.id[0], rb.id[1],
                rb.id[2], rb.powerMode, _status.displayAwake ? "on" : "NOT ON", rb.pixelFormat);
        }
    } else {
        LOG("DISPLAY", "ERROR: %s init failed", _display ? _display->controllerName() : "display");
    }

    if (_touch) {
        _touchOk = _touch->begin();
        _status.touchOk = _touchOk;
        LOG("TOUCH", "%s %s", _touch->controllerName(), _touchOk ? "initialized" : "init FAILED");
    } else {
        LOG("TOUCH", "No touch controller in profile");
    }

    LOG("BACKLIGHT", "GPIO%d %s", _profile.backlight.pin, _status.backlightDimmable ? "PWM" : "on/off only");
    return _status;
}

}  // namespace hw
