#pragma once

#include <stdint.h>

namespace config {

constexpr const char* kDefaultStarlinkHost = "192.168.100.1";

// User settings persisted in NVS. Fixed-size buffers: no heap churn, and
// the limits match what WiFi itself allows (SSID 32 bytes, PSK 64).
struct Settings {
    char wifiSsid[33] = "";
    char wifiPassword[65] = "";
    char starlinkHost[16] = "192.168.100.1";  // dotted IPv4

    bool hasWifi() const { return wifiSsid[0] != '\0'; }
};

// Loads settings, falling back to defaults for anything missing.
// Returns false if nothing has been saved yet.
bool load(Settings& out);
bool save(const Settings& settings);

}  // namespace config
