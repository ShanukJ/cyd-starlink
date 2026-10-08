#pragma once

// Display formatting for telemetry. Pure C++ (host-tested in
// test/test_format). Every formatter writes "--" for unavailable values:
// the UI never turns missing telemetry into a number.

#include <stddef.h>
#include <stdint.h>

#include <optional>

namespace ui::fmt {

constexpr const char* kUnavailable = "--";

// Throughput: "46" kbps under 1 Mbps, "4.6" Mbps under 10, "183" above.
// `unit` receives "kbps" or "Mbps" (also for unavailable values).
void throughput(const std::optional<float>& bps, char* out, size_t size, const char** unit);

// Whole milliseconds: "42".
void latency(const std::optional<float>& ms, char* out, size_t size);

// Percent of a 0..1 fraction. One decimal below 10 %, none above; tiny
// non-zero values show as "<0.1" rather than a misleading "0.0".
void percent(const std::optional<float>& fraction, char* out, size_t size);

// Compact duration: "4d 13h", "13h 05m", "5m 12s", "42s".
void duration(uint64_t seconds, char* out, size_t size);
void duration(const std::optional<uint64_t>& seconds, char* out, size_t size);

}  // namespace ui::fmt
