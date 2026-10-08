#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include "../config/Settings.h"
#include "../starlink/StarlinkService.h"
#include "WifiManager.h"

namespace net {

// The monitor's web interface on port 80, used two ways:
//   - LAN:   http://starlink-monitor.local/ (mDNS) or http://<ip>/
//   - setup: the captive portal on the setup AP (DNS answers every name
//            with the AP address; unknown URLs redirect to the page)
// One page (WebPage.h) talks to a small JSON API:
//   GET  /api/status          monitor, WiFi and dish state
//   GET  /api/settings        current settings (never the WiFi password)
//   POST /api/settings        partial update (form-encoded)
//   GET  /api/scan[?refresh]  nearby networks
//   POST /api/reboot, POST /api/factory-reset
//
// No login (typical for LAN appliances), but:
//   - state-changing requests need the "X-SM-Request" header, which a
//     cross-site form or fetch cannot send without a CORS preflight we
//     never approve (CSRF);
//   - /api/* only answers to the device's own names/addresses (DNS
//     rebinding).
// Runs entirely in the network task.
class WebUi {
public:
    struct Deps {
        config::SettingsStore* settings;
        WifiManager* wifi;
        starlink::StarlinkService* starlink;
        const char* boardName;
        uint8_t boardRotation;  // what "board default" rotation means
    };

    WebUi() : _server(80) {}

    void begin(const Deps& deps);  // after WiFi.mode(): needs the network stack
    void loop();

private:
    struct Network {
        char ssid[33];
        int8_t rssi;
        bool open;
    };

    void installRoutes();
    bool apiAllowed();     // host check for /api/*
    bool writeAllowed();   // apiAllowed() + CSRF header
    void sendJson(int code, const char* body);
    void sendError(int code, const char* message);

    void handleRoot();
    void handleStatus();
    void handleGetSettings();
    void handlePostSettings();
    void handleScan();
    void handleReboot();
    void handleFactoryReset();
    void handleNotFound();
    void collectScanResults(int16_t count);

    WebServer _server;
    DNSServer _dns;
    Deps _deps{};
    bool _dnsRunning = false;
    bool _mdnsStarted = false;

    enum class Pending : uint8_t { None, Reboot, FactoryReset };
    Pending _pending = Pending::None;
    uint32_t _pendingAt = 0;

    static constexpr size_t kMaxNetworks = 20;
    Network _networks[kMaxNetworks];
    size_t _networkCount = 0;
    bool _scanDone = false;

    char _json[1536];  // response buffer (network task only)
};

}  // namespace net
