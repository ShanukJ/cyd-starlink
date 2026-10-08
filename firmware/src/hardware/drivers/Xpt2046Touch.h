#pragma once

#include <memory>

#include "../HardwareProfile.h"
#include "../TouchDriver.h"

namespace hw {

// XPT2046 resistive touch controller. Uses LovyanGFX's Touch_XPT2046 for
// the SPI protocol and its 7-sample median filter, standalone (it is not
// attached to the display) so calibration stays in TouchMapper.
class Xpt2046Touch : public TouchDriver {
public:
    explicit Xpt2046Touch(const TouchConfig& cfg);
    ~Xpt2046Touch() override;

    bool begin() override;
    const char* controllerName() const override { return "XPT2046"; }
    bool read(RawTouch& out) override;

private:
    struct Impl;
    const TouchConfig& _cfg;
    std::unique_ptr<Impl> _impl;
};

}  // namespace hw
