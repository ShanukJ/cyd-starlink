#pragma once

#include <stdint.h>

#include <optional>

#include "Health.h"
#include "StarlinkClient.h"
#include "StarlinkStatus.h"

namespace starlink {

enum class LinkState : uint8_t {
    Waiting,      // no network yet (or setup portal open)
    Connecting,   // first attempt(s) since the network came up
    Online,       // last poll succeeded
    Unreachable,  // was trying and failing
};

// What the UI gets: link state plus the last *good* status. When the dish
// disappears the last status is kept (with its timestamp) so the UI can
// show "last seen" instead of blanking.
struct StarlinkSnapshot {
    LinkState state = LinkState::Waiting;
    Health health;
    bool hasStatus = false;
    StarlinkStatus status;
    // Highest throughput in the 15-minute history window. The live values
    // are *current traffic* (often a few kbps when idle); the peak hints at
    // what the link actually carried recently.
    std::optional<float> peakDownBps;
    std::optional<float> peakUpBps;
    uint32_t lastOkMs = 0;  // millis() of the last good poll
    uint32_t lastRttMs = 0;
    uint16_t failures = 0;  // consecutive
    ClientResult lastResult = ClientResult::Ok;
    const char* lastError = "";  // static string
    char host[16] = "";
};

}  // namespace starlink
