#pragma once

#include <memory>

#include "../DisplayDriver.h"
#include "../HardwareProfile.h"

namespace hw {

// SPI LCD panels driven through LovyanGFX. LovyanGFX is used purely as a
// panel/bus driver (init sequences, MADCTL rotation, DMA transfers); all
// drawing is done by LVGL. The LovyanGFX types stay inside the .cpp.
class LgfxSpiDisplay : public DisplayDriver {
public:
    explicit LgfxSpiDisplay(const DisplayConfig& cfg);
    ~LgfxSpiDisplay() override;

    bool begin() override;
    const char* controllerName() const override;
    bool readBack(PanelReadback& out) override;

    void setRotation(uint8_t rotation) override;
    uint8_t rotation() const override;
    uint16_t width() const override;
    uint16_t height() const override;
    uint16_t nativeWidth() const override { return _cfg.nativeWidth; }
    uint16_t nativeHeight() const override { return _cfg.nativeHeight; }

    bool wantsSwappedRgb565() const override { return true; }
    void writePixels(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* pixels) override;
    void waitIdle() override;
    void fillScreen(uint16_t rgb565) override;
    bool readPixels(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* out) override;

private:
    struct Impl;
    const DisplayConfig& _cfg;
    std::unique_ptr<Impl> _impl;
};

}  // namespace hw
