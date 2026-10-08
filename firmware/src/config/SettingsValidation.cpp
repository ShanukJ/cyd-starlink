#include "SettingsValidation.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

namespace config {

namespace {

constexpr uint8_t kMinBrightness = 10;  // never let the screen go fully dark from the web UI

bool parseUint(const char* s, unsigned long maxValue, unsigned long& out) {
    if (!s || !*s) return false;
    for (const char* p = s; *p; ++p) {
        if (!isdigit(static_cast<unsigned char>(*p))) return false;
    }
    if (strlen(s) > 10) return false;
    out = strtoul(s, nullptr, 10);
    return out <= maxValue;
}

bool validPassword(const char* p) {
    const size_t n = strlen(p);
    if (n == 0) return true;  // open network
    if (n >= 8 && n <= 63) return true;
    if (n == 64) {  // raw PSK in hex
        for (const char* c = p; *c; ++c) {
            if (!isxdigit(static_cast<unsigned char>(*c))) return false;
        }
        return true;
    }
    return false;
}

bool validPollMs(unsigned long v) {
    // History slots are 2 s; polling slower would leave gaps that look like outages.
    return v == 1000 || v == 2000;
}

}  // namespace

bool isValidIpv4(const char* s) {
    if (!s) return false;
    int parts = 0;
    const char* p = s;
    while (true) {
        if (!isdigit(static_cast<unsigned char>(*p))) return false;
        unsigned v = 0;
        int digits = 0;
        while (isdigit(static_cast<unsigned char>(*p))) {
            v = v * 10 + (*p - '0');
            if (++digits > 3 || v > 255) return false;
            ++p;
        }
        if (digits > 1 && p[-digits] == '0') return false;  // no leading zeros (octal ambiguity)
        if (++parts == 4) return *p == '\0';
        if (*p != '.') return false;
        ++p;
    }
}

UpdateResult applyUpdate(const Settings& current, const SettingsUpdate& u, Settings& out) {
    UpdateResult r;
    out = current;

    if (u.ssid) {
        const size_t n = strlen(u.ssid);
        if (n == 0 || n > 32) {
            r.error = "Network name must be 1-32 characters.";
            return r;
        }
        const char* pass = u.password ? u.password : "";
        if (!validPassword(pass)) {
            r.error = "WiFi passwords are 8-63 characters.";
            return r;
        }
        // Empty password on the same network keeps the saved one; on a
        // different network it means "open network".
        const bool sameNetwork = strcmp(u.ssid, current.wifiSsid) == 0;
        const bool keepPassword = pass[0] == '\0' && sameNetwork;
        strncpy(out.wifiSsid, u.ssid, sizeof(out.wifiSsid) - 1);
        out.wifiSsid[sizeof(out.wifiSsid) - 1] = '\0';
        if (!keepPassword) {
            strncpy(out.wifiPassword, pass, sizeof(out.wifiPassword) - 1);
            out.wifiPassword[sizeof(out.wifiPassword) - 1] = '\0';
        }
        r.wifiChanged = !sameNetwork || strcmp(out.wifiPassword, current.wifiPassword) != 0;
    }

    if (u.starlinkHost) {
        if (!isValidIpv4(u.starlinkHost)) {
            r.error = "Dish address must be an IPv4 address like 192.168.100.1.";
            return r;
        }
        strncpy(out.starlinkHost, u.starlinkHost, sizeof(out.starlinkHost) - 1);
        out.starlinkHost[sizeof(out.starlinkHost) - 1] = '\0';
    }

    unsigned long v;
    if (u.pollMs) {
        if (!parseUint(u.pollMs, 60000, v) || !validPollMs(v)) {
            r.error = "Refresh interval must be 1 or 2 seconds.";
            return r;
        }
        out.pollMs = static_cast<uint16_t>(v);
    }
    if (u.brightness) {
        if (!parseUint(u.brightness, 255, v) || v < kMinBrightness) {
            r.error = "Brightness must be 10-255.";
            return r;
        }
        out.brightness = static_cast<uint8_t>(v);
    }
    if (u.rotation) {
        if (!parseUint(u.rotation, 3, v)) {
            r.error = "Orientation must be 0-3.";
            return r;
        }
        out.rotation = static_cast<uint8_t>(v);
    }

    r.ok = true;
    return r;
}

void sanitize(Settings& s) {
    if (!isValidIpv4(s.starlinkHost)) strcpy(s.starlinkHost, kDefaultStarlinkHost);
    if (!validPollMs(s.pollMs)) s.pollMs = kDefaultPollMs;
    if (s.brightness < kMinBrightness) s.brightness = kDefaultBrightness;
    if (s.rotation > 3 && s.rotation != kRotationBoardDefault) s.rotation = kRotationBoardDefault;
    s.wifiSsid[sizeof(s.wifiSsid) - 1] = '\0';
    s.wifiPassword[sizeof(s.wifiPassword) - 1] = '\0';
}

}  // namespace config
