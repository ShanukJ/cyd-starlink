#pragma once

// ESP32-2432S028 ("Cheap Yellow Display", 2.8" 240x320) — ST7789 variant.
//
// The ESP32-2432S028 ships with either an ILI9341 or an ST7789 panel on the
// same PCB/pinout. This profile is ONLY for the ST7789 version (sold as
// "2432S028R v3", often the dual micro-USB + USB-C board, sometimes "7789"
// on the box). Panel module IDs vary by batch (see the hardware doc), so
// the variant can't be identified from RDID alone.
//
// Pin mapping cross-checked against (2026-10):
//   - LovyanGFX autodetect, _detector_Sunton_2432S028_7789_t
//   - rzeldent/platformio-espressif32-sunton, esp32-2432S028Rv3.json
//   - witnessmenow/ESP32-Cheap-Yellow-Display PINS.md (ILI9341 board, same pins)
// See docs/hardware/esp32-2432s028-st7789.md for details and open questions.
//
// Display and touch are on SEPARATE buses: the panel uses the HSPI IOMUX
// pins, the XPT2046 sits on GPIO25/32/39/33 (bit-banged, like LovyanGFX).
// VSPI pins 18/19/23/5 are wired to the TF card slot and left free.

#include <driver/spi_common.h>

#include "../HardwareProfile.h"

namespace hw {

inline constexpr HardwareProfile ESP32_2432S028_ST7789 = {
    .id = "esp32-2432s028-st7789",
    .name = "ESP32-2432S028 ST7789",
    .display = {
        .controller = DisplayController::ST7789,
        .nativeWidth = 240,
        .nativeHeight = 320,
        .spi = {.host = SPI2_HOST, .sclk = 14, .mosi = 13, .miso = 12},
        .cs = 15,
        .dc = 2,
        .rst = -1,             // panel reset is tied to the ESP32 EN line
        .spiMode = 0,
        .writeHz = 40000000,   // LovyanGFX autodetect uses 80 MHz; 40 MHz for margin
        .readHz = 16000000,
        .invert = false,       // verified on hardware (colour bars)
        .bgr = true,           // verified on hardware (colour bars)
    },
    .touch = {
        .controller = TouchController::XPT2046,
        .spi = {.host = -1, .sclk = 25, .mosi = 32, .miso = 39},
        .cs = 33,
        .irq = 36,             // PENIRQ, active low (input-only pin, external pull-up)
        .spiHz = 1000000,
        // Derived from LovyanGFX's ST7789 CYD config (x 300..3900, y 3700..200,
        // touch offset_rotation 2) re-expressed in native panel coordinates.
        // Verified on hardware in all four rotations.
        .calibration = {
            .rawXMin = 3900,
            .rawXMax = 300,
            .rawYMin = 200,
            .rawYMax = 3700,
            .swapXY = false,
        },
    },
    .backlight = {
        .pin = 21,
        .activeHigh = true,
        .pwmHz = 12000,
        .pwmBits = 8,
        .defaultLevel = 204,  // 80%
    },
    .button = {.pin = 0, .activeLow = true},  // BOOT button (external pull-up)
    .defaultRotation = 0,  // portrait 240x320, USB connectors at the bottom (verified)
};

}  // namespace hw
