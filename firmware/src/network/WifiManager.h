#pragma once

#include <atomic>
#include <mutex>

#include "../config/Settings.h"
#include "WifiStatus.h"

namespace net {

// Owns the WiFi radio. Driven from the network task (app/NetworkTask) so
// association, DHCP and scans never stall the UI. The web UI (WebUi) is
// separate and serves both the setup portal and the LAN settings page.
//
// Station: connect with the saved credentials; on failure or link loss
// retry with exponential backoff (2 s .. 60 s). New credentials saved
// anywhere (web UI) are picked up from the SettingsStore.
// Setup AP (WPA2, random password shown on screen) opens when:
//   - no network is configured,
//   - the saved network failed 3 times and never connected since boot or
//     since the credentials last changed,
//   - requestPortal() is called (BOOT button).
class WifiManager {
public:
    static constexpr const char* kHostname = "starlink-monitor";

    void begin(config::SettingsStore& settings);

    // --- Network task only
    void loop();
    bool online() const { return _sta == StaState::Connected; }
    bool portalActive() const { return _apActive; }
    const char* apIp() const { return _apIp; }

    // --- Any task
    WifiStatus status() const;  // thread-safe snapshot
    void requestPortal() { _portalRequested = true; }
    void requestPortalClose() { _portalCloseRequested = true; }

private:
    void step(uint32_t now);
    void checkCredentials(uint32_t now);
    void startConnect(uint32_t now);
    void onConnectFailed(uint32_t now, const char* why);
    void startPortal(uint32_t now, const char* why);
    void stopPortal(const char* why);
    void publish();

    config::SettingsStore* _store = nullptr;
    uint32_t _settingsVersion = 0;
    char _ssid[33] = "";
    char _password[65] = "";

    // Network-task state
    StaState _sta = StaState::NotConfigured;
    uint32_t _attemptStart = 0;
    uint32_t _nextAttempt = 0;
    uint16_t _failures = 0;
    bool _everConnected = false;
    bool _savePending = false;          // new credentials: connect even if a phone is on the AP
    bool _closePortalOnConnect = false; // portal was used to save; close it once online
    uint32_t _connectedAt = 0;
    bool _apActive = false;
    uint32_t _portalLastActivity = 0;
    uint32_t _portalRetryAt = 0;
    uint8_t _portalClients = 0;
    char _apIp[16] = "";
    const char* _lastError = "";
    uint32_t _lastRssiPoll = 0;
    int8_t _rssi = 0;

    std::atomic<bool> _portalRequested{false};
    std::atomic<bool> _portalCloseRequested{false};

    mutable std::mutex _mutex;
    WifiStatus _status;  // guarded by _mutex
};

}  // namespace net
