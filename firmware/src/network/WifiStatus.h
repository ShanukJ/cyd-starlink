#pragma once

#include <stdint.h>

namespace net {

enum class StaState : uint8_t {
    NotConfigured,  // no saved network
    Connecting,     // association/DHCP in progress
    Connected,
    WaitingRetry,   // last attempt failed or link dropped; backing off
};

// Snapshot of the WiFi state, safe to copy across tasks. The UI only ever
// sees this; it never calls the WiFi API itself.
struct WifiStatus {
    StaState sta = StaState::NotConfigured;
    char ssid[33] = "";            // network being joined / joined
    uint32_t ip = 0;               // IPv4, network byte order (0 = none)
    int8_t rssi = 0;               // dBm, valid when Connected
    uint16_t failures = 0;         // consecutive failed attempts
    uint32_t retryInMs = 0;        // valid when WaitingRetry
    const char* lastError = "";    // static string, e.g. "AUTH_FAIL"

    bool portalActive = false;
    char apSsid[33] = "";
    char apPassword[16] = "";
    char apIp[16] = "";
    uint8_t portalClients = 0;
};

}  // namespace net
