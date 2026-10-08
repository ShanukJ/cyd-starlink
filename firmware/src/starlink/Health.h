#pragma once

#include <stdint.h>

namespace starlink {

struct StarlinkSnapshot;

// The single overall state shown to the user.
enum class HealthState : uint8_t {
    Connecting,  // no answer from the dish yet
    Online,      // dish reachable and connected to the network
    Degraded,    // connected, but something needs attention
    Offline,     // dish unreachable, or reachable but not connected
    Error,       // dish answers but can't be used (account, wrong device, bad data)
};

struct Health {
    HealthState state = HealthState::Connecting;
    const char* reason = "";  // static, human-readable; "" when Online
};

const char* healthStateName(HealthState s);  // "ONLINE", ...

// Thresholds are deliberately conservative to avoid flapping on noisy samples.
constexpr float kDegradedDropRate = 0.10f;  // >= 10 % ping loss

// Pure function of the snapshot; unit-tested on the host.
Health evaluateHealth(const StarlinkSnapshot& s);

}  // namespace starlink
