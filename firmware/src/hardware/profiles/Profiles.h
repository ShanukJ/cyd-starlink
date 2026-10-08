#pragma once

// Selects the hardware profile for this build. Each PlatformIO environment
// defines exactly one HW_PROFILE_* flag.

#if defined(HW_PROFILE_ESP32_2432S028_ST7789)
#include "esp32_2432s028_st7789.h"
namespace hw {
inline constexpr const HardwareProfile& kActiveProfile = ESP32_2432S028_ST7789;
}
#else
#error "No hardware profile selected. Build with -D HW_PROFILE_<BOARD> (see platformio.ini)."
#endif
