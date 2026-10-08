#pragma once

#include <stdint.h>

#include "StarlinkStatus.h"

namespace starlink {

// Turns a StarlinkStatus into a fixed list of named checks for the
// diagnostics page. Pure C++ (host-tested in test/test_diagnostics).
//
// Every dish alert is routed to exactly one check, so nothing is hidden:
// alerts without a natural home (water, mast, location, motors, and alerts
// added in newer dish firmware) land in "ALERTS".

enum class CheckLevel : uint8_t {
    Ok,       // reported and fine
    Info,     // noteworthy but not a problem (e.g. heating, update downloading)
    Warn,     // needs attention
    Fail,     // not working
    Unknown,  // not reported by the dish
};

enum class CheckId : uint8_t {
    Hardware,
    Rf,
    Gps,
    Network,
    Thermal,
    Obstruction,
    Ethernet,
    Alerts,
    Software,
    Count,
};

constexpr int kCheckCount = static_cast<int>(CheckId::Count);

struct Check {
    const char* name = "";  // "GPS", ...
    CheckLevel level = CheckLevel::Unknown;
    char detail[28] = "--";  // short: "10 satellites", "Throttling"
};

void evaluateChecks(const StarlinkStatus& s, Check out[kCheckCount]);

}  // namespace starlink
