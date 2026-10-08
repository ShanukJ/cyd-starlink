#include "LgfxSpiDisplay.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

namespace hw {

namespace {
constexpr uint8_t CMD_RDDPM = 0x0A;
constexpr uint8_t CMD_RDDCOLMOD = 0x0C;
constexpr uint8_t CMD_RDID1 = 0xDA;
constexpr uint8_t CMD_RDID2 = 0xDB;
constexpr uint8_t CMD_RDID3 = 0xDC;
}  // namespace

struct LgfxSpiDisplay::Impl {
    lgfx::LGFX_Device device;
    lgfx::Bus_SPI bus;
    std::unique_ptr<lgfx::Panel_LCD> panel;
};

LgfxSpiDisplay::LgfxSpiDisplay(const DisplayConfig& cfg) : _cfg(cfg), _impl(new Impl) {}

LgfxSpiDisplay::~LgfxSpiDisplay() = default;

const char* LgfxSpiDisplay::controllerName() const {
    switch (_cfg.controller) {
        case DisplayController::ST7789: return "ST7789";
    }
    return "unknown";
}

bool LgfxSpiDisplay::begin() {
    switch (_cfg.controller) {
        case DisplayController::ST7789: _impl->panel.reset(new lgfx::Panel_ST7789()); break;
    }
    if (!_impl->panel) return false;

    auto b = _impl->bus.config();
    b.spi_host = static_cast<spi_host_device_t>(_cfg.spi.host);
    b.spi_mode = _cfg.spiMode;
    b.freq_write = _cfg.writeHz;
    b.freq_read = _cfg.readHz;
    b.spi_3wire = false;
    b.use_lock = true;
    b.dma_channel = SPI_DMA_CH_AUTO;
    b.pin_sclk = _cfg.spi.sclk;
    b.pin_mosi = _cfg.spi.mosi;
    b.pin_miso = _cfg.spi.miso;
    b.pin_dc = _cfg.dc;
    _impl->bus.config(b);
    _impl->panel->setBus(&_impl->bus);

    auto p = _impl->panel->config();
    p.pin_cs = _cfg.cs;
    p.pin_rst = _cfg.rst;
    p.pin_busy = -1;
    p.panel_width = p.memory_width = _cfg.nativeWidth;
    p.panel_height = p.memory_height = _cfg.nativeHeight;
    p.offset_x = 0;
    p.offset_y = 0;
    p.offset_rotation = 0;
    p.readable = _cfg.spi.miso >= 0;
    p.invert = _cfg.invert;
    p.rgb_order = !_cfg.bgr;
    p.dlen_16bit = false;
    p.bus_shared = false;  // touch has its own bus on every supported board so far
    _impl->panel->config(p);

    _impl->device.setPanel(_impl->panel.get());
    return _impl->device.init();
}

bool LgfxSpiDisplay::readBack(PanelReadback& out) {
    if (!_impl->panel || _cfg.spi.miso < 0) return false;
    waitIdle();
    auto& panel = *_impl->panel;

    // ST7789 8-bit register reads have no dummy clock, but LovyanGFX
    // defaults to one dummy bit (correct for 24/32-bit reads such as RDDID).
    // Verified on hardware: with 1 bit every byte comes back shifted.
    auto cfg = panel.config();
    const uint8_t savedDummy = cfg.dummy_read_bits;
    cfg.dummy_read_bits = 0;
    panel.config(cfg);

    out.id[0] = panel.readCommand(CMD_RDID1, 0, 1);
    out.id[1] = panel.readCommand(CMD_RDID2, 0, 1);
    out.id[2] = panel.readCommand(CMD_RDID3, 0, 1);
    out.powerMode = panel.readCommand(CMD_RDDPM, 0, 1);
    out.pixelFormat = panel.readCommand(CMD_RDDCOLMOD, 0, 1);

    cfg.dummy_read_bits = savedDummy;
    panel.config(cfg);
    return true;
}

void LgfxSpiDisplay::setRotation(uint8_t rotation) {
    waitIdle();
    _impl->device.setRotation(rotation & 3);
}

uint8_t LgfxSpiDisplay::rotation() const { return _impl->device.getRotation(); }
uint16_t LgfxSpiDisplay::width() const { return _impl->device.width(); }
uint16_t LgfxSpiDisplay::height() const { return _impl->device.height(); }

void LgfxSpiDisplay::writePixels(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* pixels) {
    // Keep the SPI transaction open between frames so pushImageDMA() returns
    // as soon as the transfer is queued. The bus is not shared, so holding
    // it is safe; waitIdle() provides the completion barrier.
    auto& d = _impl->device;
    if (d.getStartCount() == 0) d.startWrite();
    d.pushImageDMA(x, y, w, h, reinterpret_cast<const lgfx::swap565_t*>(pixels));
}

void LgfxSpiDisplay::waitIdle() { _impl->device.waitDMA(); }

bool LgfxSpiDisplay::readPixels(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t* out) {
    if (_cfg.spi.miso < 0) return false;
    waitIdle();
    _impl->device.readRect(x, y, w, h, reinterpret_cast<lgfx::rgb565_t*>(out));
    return true;
}

void LgfxSpiDisplay::fillScreen(uint16_t rgb565) {
    waitIdle();
    _impl->device.fillScreen(rgb565);
}

}  // namespace hw
