#pragma once

#include <stdint.h>

// Shared colour palette. State is never conveyed by colour alone: every
// coloured status also carries a symbol and text.
namespace ui::theme {

constexpr uint32_t kBg = 0x0B0F14;
constexpr uint32_t kText = 0xE6EDF3;
constexpr uint32_t kMuted = 0x8B949E;
constexpr uint32_t kOk = 0x3FB950;
constexpr uint32_t kWarn = 0xD29922;
constexpr uint32_t kFail = 0xF85149;
constexpr uint32_t kDivider = 0x30363D;
constexpr uint32_t kCard = 0x151B23;
constexpr uint32_t kAccentDown = 0x58A6FF;
constexpr uint32_t kAccentUp = 0xBC8CFF;

}  // namespace ui::theme
