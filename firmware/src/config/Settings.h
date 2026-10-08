#pragma once

#include <stdint.h>

#include <atomic>
#include <mutex>

namespace config {

constexpr const char* kDefaultStarlinkHost = "192.168.100.1";
constexpr uint16_t kDefaultPollMs = 2000;
constexpr uint8_t kDefaultBrightness = 204;  // 80 %
constexpr uint8_t kRotationBoardDefault = 0xFF;

// User settings persisted in NVS. Fixed-size buffers: no heap churn, and
// the limits match what WiFi itself allows (SSID 32 bytes, PSK 64).
struct Settings {
    char wifiSsid[33] = "";
    char wifiPassword[65] = "";
    char starlinkHost[16] = "192.168.100.1";  // dotted IPv4
    uint16_t pollMs = kDefaultPollMs;         // 1000 or 2000 (see SettingsValidation)
    uint8_t brightness = kDefaultBrightness;  // 10..255
    uint8_t rotation = kRotationBoardDefault; // 0..3, or board default

    bool hasWifi() const { return wifiSsid[0] != '\0'; }
};

// Loads settings, falling back to defaults for anything missing.
// Returns false if nothing has been saved yet.
bool load(Settings& out);
bool save(const Settings& settings);
// Erases every saved setting (WiFi included).
bool erase();

// The single shared copy of the settings. Any task may read; writers
// persist to NVS. version() changes on every successful update so other
// tasks can notice changes by polling it cheaply.
class SettingsStore {
public:
    void begin();  // loads from NVS
    Settings get() const;
    uint32_t version() const { return _version.load(); }
    bool update(const Settings& s);  // validated by the caller; persists
    bool factoryReset();             // erases NVS; caller should reboot

private:
    mutable std::mutex _mutex;
    Settings _settings;  // guarded by _mutex
    std::atomic<uint32_t> _version{0};
};

}  // namespace config
