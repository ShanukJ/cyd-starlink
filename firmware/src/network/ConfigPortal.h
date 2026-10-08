#pragma once

#include <DNSServer.h>
#include <WebServer.h>

#include <functional>

#include "../config/Settings.h"
#include "WifiStatus.h"

namespace net {

// Setup web UI served on the soft-AP, with a captive-portal DNS so phones
// open it automatically. Runs entirely in the network task.
//
//   GET  /         setup page (static HTML + JS)
//   GET  /config   current settings as JSON (never includes the password)
//   GET  /scan     nearby networks as JSON (?refresh=1 rescans)
//   POST /save     validate + apply new settings
//   GET  /status   connection progress as JSON (polled by the page)
//   *              302 to / (captive-portal detection)
class ConfigPortal {
public:
    using SaveFn = std::function<void(const config::Settings&)>;
    using StatusFn = std::function<WifiStatus()>;

    ConfigPortal() : _server(80) {}

    void start(const config::Settings* current, SaveFn onSave, StatusFn status);
    void stop();
    void loop();
    bool active() const { return _active; }

private:
    struct Network {
        char ssid[33];
        int8_t rssi;
        bool open;
    };

    void installRoutes();
    void handleRoot();
    void handleConfig();
    void handleScan();
    void handleSave();
    void handleStatus();
    void handleNotFound();
    void collectScanResults(int16_t count);
    void sendJson(int code, const String& body);

    WebServer _server;
    DNSServer _dns;
    bool _active = false;
    bool _routesInstalled = false;
    const config::Settings* _current = nullptr;
    SaveFn _onSave;
    StatusFn _status;

    static constexpr size_t kMaxNetworks = 20;
    Network _networks[kMaxNetworks];
    size_t _networkCount = 0;
    bool _scanDone = false;
};

}  // namespace net
