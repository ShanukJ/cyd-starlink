#pragma once

#include <stdint.h>

namespace hw {

// Values read back from the panel controller (MIPI DCS registers), so the
// firmware can verify the panel instead of assuming it works.
struct PanelReadback {
    uint8_t id[3];        // RDID1..3 (DAh..DCh). Vendor-programmed: varies by module, not by controller.
    uint8_t powerMode;    // RDDPM (0Ah). 0x9C = booster on, awake, normal mode, display on.
    uint8_t pixelFormat;  // RDDCOLMOD (0Ch). 0x05 = 16 bit/pixel.
};

// Abstract display panel. The UI layer (LVGL port) only talks to this.
//
// Rotation convention (matches LovyanGFX), with (nx, ny) a native panel
// coordinate in a native W x H panel and (x, y) the rotated coordinate:
//   0: x = nx,         y = ny          (native, W x H)
//   1: x = ny,         y = W - 1 - nx  (H x W)
//   2: x = W - 1 - nx, y = H - 1 - ny  (W x H)
//   3: x = H - 1 - ny, y = nx          (H x W)
// Touch mapping (TouchMapper) uses the same table, so any driver that
// honours it gets correct touch coordinates in every orientation.
class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;

    virtual bool begin() = 0;
    virtual const char* controllerName() const = 0;

    // Returns false when the bus has no read path (MISO not wired).
    virtual bool readBack(PanelReadback& out) = 0;

    virtual void setRotation(uint8_t rotation) = 0;
    virtual uint8_t rotation() const = 0;
    virtual uint16_t width() const = 0;   // for the current rotation
    virtual uint16_t height() const = 0;
    virtual uint16_t nativeWidth() const = 0;
    virtual uint16_t nativeHeight() const = 0;

    // Whether writePixels() expects big-endian RGB565 (true for SPI panels).
    virtual bool wantsSwappedRgb565() const = 0;

    // Starts writing a w x h block of RGB565 pixels at (x, y). May return
    // before the transfer is complete: `pixels` must stay untouched until
    // waitIdle() returns. `pixels` must be in DMA-capable memory.
    virtual void writePixels(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* pixels) = 0;
    virtual void waitIdle() = 0;

    virtual void fillScreen(uint16_t rgb565) = 0;

    // Reads back a block of displayed pixels as RGB565 (current rotation).
    // Used for screenshots. Returns false without a read path (no MISO).
    virtual bool readPixels(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* out) = 0;
};

}  // namespace hw
