#pragma once

#include <atomic>
#include <mutex>

#include "../config/Settings.h"
#include "ConfigPortal.h"
#include "WifiStatus.h"

namespace net {

// Owns the WiFi radio. Driven from the network task (app/NetworkTask) so
// association, DHCP, scans and the setup web server never stall the UI.
//
// Station: connect with saved credentials; on failure or link loss retry
// with exponential backoff (2 s .. 60 s).
// Setup portal (WPA2 soft-AP + captive DNS + web form) opens when:
//   - no network is configured,
//   - the saved network failed 3 times and never connected this boot,
//   - requestPortal() is called (BOOT button).
class WifiManager {
public:
    // Takes a copy of the settings; from here on the network task owns them.
    void begin(const config::Settings& settings);

    // --- Network task only
    void loop();
    bool online() const { return _sta == StaState::Connected; }
    bool portalActive() const { return _portal.active(); }
    const config::Settings& settings() const { return _settings; }

    // --- Any task
    WifiStatus status() const;  // thread-safe snapshot
    void requestPortal() { _portalRequested = true; }
    void requestPortalClose() { _portalCloseRequested = true; }

private:
    void step(uint32_t now);
    void startConnect(uint32_t now);
    void onConnectFailed(uint32_t now, const char* why);
    void startPortal(uint32_t now, const char* why);
    void stopPortal(const char* why);
    void applySettings(const config::Settings& s);
    void publish();

    config::Settings _settings;
    ConfigPortal _portal;

    // Network-task state
    StaState _sta = StaState::NotConfigured;
    uint32_t _attemptStart = 0;
    uint32_t _nextAttempt = 0;
    uint16_t _failures = 0;
    bool _everConnected = false;
    bool _savePending = false;          // user just saved: connect even if a phone is on the AP
    bool _closePortalOnConnect = false; // portal was used to save; close it once online
    uint32_t _connectedAt = 0;
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
