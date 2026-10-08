#include "Xpt2046Touch.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

namespace hw {

struct Xpt2046Touch::Impl {
    lgfx::Touch_XPT2046 touch;
};

Xpt2046Touch::Xpt2046Touch(const TouchConfig& cfg) : _cfg(cfg), _impl(new Impl) {}

Xpt2046Touch::~Xpt2046Touch() = default;

bool Xpt2046Touch::begin() {
    auto c = _impl->touch.config();
    c.spi_host = _cfg.spi.host;  // -1 = software SPI
    c.pin_sclk = _cfg.spi.sclk;
    c.pin_mosi = _cfg.spi.mosi;
    c.pin_miso = _cfg.spi.miso;
    c.pin_cs = _cfg.cs;
    c.freq = _cfg.spiHz;
    c.bus_shared = false;
    // PENIRQ is deliberately not used to gate reads: polling every LVGL
    // input period is cheap, and it keeps touch working on boards where the
    // IRQ line is missing or floating. The hardware test shows its level.
    c.pin_int = -1;
    _impl->touch.config(c);
    return _impl->touch.init();
}

bool Xpt2046Touch::read(RawTouch& out) {
    lgfx::touch_point_t tp;
    if (_impl->touch.getTouchRaw(&tp, 1) == 0) return false;
    out.x = tp.x;
    out.y = tp.y;
    out.pressure = tp.size;
    return true;
}

}  // namespace hw
