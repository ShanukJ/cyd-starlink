#include "Settings.h"

#include <Preferences.h>
#include <string.h>

#include "../utils/Log.h"
#include "SettingsValidation.h"

namespace config {

namespace {

constexpr const char* kNamespace = "sm-config";
// Bump when the stored layout changes, and migrate in load().
//   1: WiFi SSID/password, Starlink host
//   2: + poll interval, brightness, rotation (absent keys -> defaults)
constexpr uint8_t kSchemaVersion = 2;

constexpr const char* kKeySchema = "schema";
constexpr const char* kKeySsid = "wifi_ssid";
constexpr const char* kKeyPassword = "wifi_pass";
constexpr const char* kKeyStarlinkHost = "sl_host";
constexpr const char* kKeyPollMs = "poll_ms";
constexpr const char* kKeyBrightness = "bright";
constexpr const char* kKeyRotation = "rotation";

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
    // Schema 2 keys; on a schema-1 device they are simply absent.
    out.pollMs = prefs.getUShort(kKeyPollMs, kDefaultPollMs);
    out.brightness = prefs.getUChar(kKeyBrightness, kDefaultBrightness);
    out.rotation = prefs.getUChar(kKeyRotation, kRotationBoardDefault);
    prefs.end();

    // Never trust stored values blindly (older firmware, corruption).
    sanitize(out);
    LOG("CONFIG", "Loaded (schema %u): WiFi \"%s\", Starlink %s, poll %u ms", schema, out.wifiSsid,
        out.starlinkHost, out.pollMs);
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
    ok &= prefs.putUShort(kKeyPollMs, s.pollMs) == 2;
    ok &= prefs.putUChar(kKeyBrightness, s.brightness) == 1;
    ok &= prefs.putUChar(kKeyRotation, s.rotation) == 1;
    prefs.end();
    LOG("CONFIG", "%s", ok ? "Saved" : "ERROR: save failed");
    return ok;
}

bool erase() {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) return false;
    const bool ok = prefs.clear();
    prefs.end();
    LOG("CONFIG", "%s", ok ? "All settings erased" : "ERROR: erase failed");
    return ok;
}

void SettingsStore::begin() {
    Settings s;
    load(s);
    std::lock_guard<std::mutex> lock(_mutex);
    _settings = s;
    _version++;
}

Settings SettingsStore::get() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _settings;
}

bool SettingsStore::update(const Settings& s) {
    const bool ok = save(s);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _settings = s;  // apply even if NVS failed: better than ignoring the user
    }
    _version++;
    return ok;
}

bool SettingsStore::factoryReset() {
    const bool ok = erase();
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _settings = Settings{};
    }
    _version++;
    return ok;
}

}  // namespace config
