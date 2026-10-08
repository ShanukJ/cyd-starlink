#pragma once

// Validation shared by the web UI and settings loading. Pure C++ (no
// Arduino), host-tested in test/test_settings.

#include "Settings.h"

namespace config {

// A partial update as submitted by the web UI: only fields that are
// present are changed. Strings are raw user input.
struct SettingsUpdate {
    const char* ssid = nullptr;
    const char* password = nullptr;  // "" with the same SSID = keep saved password
    const char* starlinkHost = nullptr;
    const char* pollMs = nullptr;
    const char* brightness = nullptr;
    const char* rotation = nullptr;
};

struct UpdateResult {
    bool ok = false;
    const char* error = "";      // human-readable, for the web UI
    bool wifiChanged = false;    // SSID or password changed: reconnect needed
};

// Applies `u` on top of `current` into `out`, validating every field
// present. On error `out` is unspecified and nothing should be saved.
UpdateResult applyUpdate(const Settings& current, const SettingsUpdate& u, Settings& out);

// Replaces out-of-range values with defaults (for values read from NVS).
void sanitize(Settings& s);

bool isValidIpv4(const char* s);

}  // namespace config
