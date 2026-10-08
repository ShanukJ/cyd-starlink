#pragma once

// A HardwareProfile is a plain description of one physical board: which
// controllers it has, which pins they use, and the board-specific quirks
// (colour order, inversion, touch calibration). It contains no code.
//
// Adding a new board = adding a new profile header in profiles/ and a new
// PlatformIO environment. Drivers read everything they need from here.

#include <stdint.h>

namespace hw {

enum class DisplayController : uint8_t {
    ST7789,
};

enum class TouchController : uint8_t {
    None,
    XPT2046,
};

// spiHost: an ESP-IDF spi_host_device_t value (SPI2_HOST / SPI3_HOST), or
// -1 for bit-banged software SPI.
struct SpiPins {
    int8_t host;
    int8_t sclk;
    int8_t mosi;
    int8_t miso;  // -1 if not wired
};

struct DisplayConfig {
    DisplayController controller;
    uint16_t nativeWidth;   // in the controller's native orientation (rotation 0)
    uint16_t nativeHeight;
    SpiPins spi;
    int8_t cs;
    int8_t dc;
    int8_t rst;             // -1 when tied to EN / not controllable
    uint8_t spiMode;
    uint32_t writeHz;
    uint32_t readHz;
    bool invert;            // send INVON
    bool bgr;               // set MADCTL BGR bit
};

// Linear map from raw touch-controller readings to *native* panel pixels
// (display rotation 0). rawXMin maps to x = 0, rawXMax to x = nativeWidth-1;
// min may be larger than max to flip an axis. swapXY is applied first, for
// controllers whose X/Y channels are wired to the panel's Y/X axes.
struct TouchCalibration {
    int16_t rawXMin;
    int16_t rawXMax;
    int16_t rawYMin;
    int16_t rawYMax;
    bool swapXY;
};

struct TouchConfig {
    TouchController controller;
    SpiPins spi;
    int8_t cs;
    int8_t irq;             // -1 if not wired
    uint32_t spiHz;
    TouchCalibration calibration;
};

struct BacklightConfig {
    int8_t pin;             // -1 if always on
    bool activeHigh;
    uint32_t pwmHz;
    uint8_t pwmBits;
    uint8_t defaultLevel;   // 0..255
};

// A physical push button usable at runtime (e.g. the BOOT button).
struct ButtonConfig {
    int8_t pin;             // -1 if none
    bool activeLow;
};

struct HardwareProfile {
    const char* id;         // stable machine id, e.g. "esp32-2432s028-st7789"
    const char* name;       // human readable
    DisplayConfig display;
    TouchConfig touch;
    BacklightConfig backlight;
    ButtonConfig button;    // opens WiFi setup when held
    uint8_t defaultRotation;  // see DisplayDriver.h for the rotation convention
};

}  // namespace hw
