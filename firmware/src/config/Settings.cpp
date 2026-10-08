#include "Settings.h"

#include <Preferences.h>
#include <string.h>

#include "../utils/Log.h"

namespace config {

namespace {

constexpr const char* kNamespace = "sm-config";
// Bump when the stored layout changes, and migrate in load().
constexpr uint8_t kSchemaVersion = 1;

constexpr const char* kKeySchema = "schema";
constexpr const char* kKeySsid = "wifi_ssid";
constexpr const char* kKeyPassword = "wifi_pass";
constexpr const char* kKeyStarlinkHost = "sl_host";

}  // namespace

bool load(Settings& out) {
    out = Settings{};
    Preferences prefs;
    // Opened read-write: a read-only open of a namespace that doesn't exist
    // yet (first boot) fails and makes the core log a spurious error.
    if (!prefs.begin(kNamespace, false)) {
        LOG("CONFIG", "ERROR: cannot open NVS - using defaults");
        return false;
    }
    const uint8_t schema = prefs.getUChar(kKeySchema, 0);
    if (schema == 0) {
        prefs.end();
        LOG("CONFIG", "No saved settings - using defaults");
        return false;
    }
    prefs.getString(kKeySsid, out.wifiSsid, sizeof(out.wifiSsid));
    prefs.getString(kKeyPassword, out.wifiPassword, sizeof(out.wifiPassword));
    if (prefs.isKey(kKeyStarlinkHost)) {
        prefs.getString(kKeyStarlinkHost, out.starlinkHost, sizeof(out.starlinkHost));
    }
    prefs.end();
    LOG("CONFIG", "Loaded (schema %u): WiFi \"%s\", Starlink %s", schema, out.wifiSsid, out.starlinkHost);
    return true;
}

bool save(const Settings& s) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        LOG("CONFIG", "ERROR: cannot open NVS");
        return false;
    }
    bool ok = prefs.putUChar(kKeySchema, kSchemaVersion) == 1;
    ok &= prefs.putString(kKeySsid, s.wifiSsid) == strlen(s.wifiSsid);
    ok &= prefs.putString(kKeyPassword, s.wifiPassword) == strlen(s.wifiPassword);
    ok &= prefs.putString(kKeyStarlinkHost, s.starlinkHost) == strlen(s.starlinkHost);
    prefs.end();
    LOG("CONFIG", "%s", ok ? "Saved" : "ERROR: save failed");
    return ok;
}

}  // namespace config
