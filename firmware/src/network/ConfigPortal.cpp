#include "ConfigPortal.h"

#include <WiFi.h>

#include <algorithm>

#include "../utils/Log.h"
#include "PortalPage.h"

namespace net {

namespace {

constexpr uint16_t kDnsPort = 53;

// JSON string literal with escaping. SSIDs are arbitrary bytes and must
// never be able to break out of the string.
void appendJsonString(String& out, const char* s) {
    out += '"';
    for (; *s; ++s) {
        const char c = *s;
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<uint8_t>(c) < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += c;
        }
    }
    out += '"';
}

const char* staName(StaState s) {
    switch (s) {
        case StaState::NotConfigured: return "unconfigured";
        case StaState::Connecting: return "connecting";
        case StaState::Connected: return "connected";
        case StaState::WaitingRetry: return "retry";
    }
    return "unknown";
}

bool validPassword(const String& p) {
    if (p.isEmpty()) return true;  // open network
    if (p.length() >= 8 && p.length() <= 63) return true;
    if (p.length() == 64) {  // raw PSK in hex
        for (char c : p) {
            if (!isxdigit(static_cast<unsigned char>(c))) return false;
        }
        return true;
    }
    return false;
}

}  // namespace

void ConfigPortal::start(const config::Settings* current, SaveFn onSave, StatusFn status) {
    _current = current;
    _onSave = std::move(onSave);
    _status = std::move(status);
    _scanDone = false;
    _networkCount = 0;

    installRoutes();
    _dns.setErrorReplyCode(DNSReplyCode::NoError);
    _dns.start(kDnsPort, "*", WiFi.softAPIP());
    _server.begin();
    WiFi.scanNetworks(true);
    _active = true;
}

void ConfigPortal::stop() {
    if (!_active) return;
    _server.stop();
    _dns.stop();
    WiFi.scanDelete();
    _active = false;
}

void ConfigPortal::loop() {
    if (!_active) return;
    _dns.processNextRequest();
    _server.handleClient();
}

void ConfigPortal::installRoutes() {
    if (_routesInstalled) return;
    _routesInstalled = true;
    _server.on("/", HTTP_GET, [this] { handleRoot(); });
    _server.on("/config", HTTP_GET, [this] { handleConfig(); });
    _server.on("/scan", HTTP_GET, [this] { handleScan(); });
    _server.on("/save", HTTP_POST, [this] { handleSave(); });
    _server.on("/status", HTTP_GET, [this] { handleStatus(); });
    _server.onNotFound([this] { handleNotFound(); });
}

void ConfigPortal::sendJson(int code, const String& body) {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(code, "application/json", body);
}

void ConfigPortal::handleRoot() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send_P(200, "text/html", kPortalPage, sizeof(kPortalPage) - 1);
}

void ConfigPortal::handleConfig() {
    String body = "{\"ssid\":";
    appendJsonString(body, _current->wifiSsid);
    body += ",\"hasPassword\":";
    body += _current->wifiPassword[0] ? "true" : "false";
    body += ",\"host\":";
    appendJsonString(body, _current->starlinkHost);
    body += '}';
    sendJson(200, body);
}

void ConfigPortal::collectScanResults(int16_t count) {
    _networkCount = 0;
    for (int16_t i = 0; i < count; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;  // hidden network
        const int8_t rssi = WiFi.RSSI(i);
        // De-duplicate (mesh / multiple APs): keep the strongest.
        Network* existing = nullptr;
        for (size_t j = 0; j < _networkCount; ++j) {
            if (ssid == _networks[j].ssid) existing = &_networks[j];
        }
        if (existing) {
            existing->rssi = std::max(existing->rssi, rssi);
            continue;
        }
        if (_networkCount == kMaxNetworks) continue;
        Network& n = _networks[_networkCount++];
        strlcpy(n.ssid, ssid.c_str(), sizeof(n.ssid));
        n.rssi = rssi;
        n.open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    }
    std::sort(_networks, _networks + _networkCount, [](const Network& a, const Network& b) { return a.rssi > b.rssi; });
    WiFi.scanDelete();
    _scanDone = true;
}

void ConfigPortal::handleScan() {
    if (_server.hasArg("refresh")) {
        _scanDone = false;
        WiFi.scanDelete();
        WiFi.scanNetworks(true);
    }
    if (!_scanDone) {
        const int16_t n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) {
            sendJson(200, "{\"state\":\"scanning\"}");
            return;
        }
        if (n < 0) {
            // Failed or never started (e.g. the station was mid-connect).
            WiFi.scanNetworks(true);
            sendJson(200, "{\"state\":\"failed\"}");
            return;
        }
        collectScanResults(n);
    }
    String body = "{\"state\":\"done\",\"networks\":[";
    for (size_t i = 0; i < _networkCount; ++i) {
        if (i) body += ',';
        body += "{\"ssid\":";
        appendJsonString(body, _networks[i].ssid);
        body += ",\"rssi\":";
        body += _networks[i].rssi;
        body += ",\"open\":";
        body += _networks[i].open ? "true" : "false";
        body += '}';
    }
    body += "]}";
    sendJson(200, body);
}

void ConfigPortal::handleSave() {
    const String ssid = _server.arg("ssid");
    const String pass = _server.arg("pass");
    String host = _server.arg("host");
    host.trim();

    auto fail = [this](const char* msg) {
        String body = "{\"ok\":false,\"error\":";
        appendJsonString(body, msg);
        body += '}';
        sendJson(400, body);
    };

    if (ssid.isEmpty() || ssid.length() > 32) return fail("Network name must be 1-32 characters.");
    if (!validPassword(pass)) return fail("WiFi passwords are 8-63 characters.");
    IPAddress ip;
    if (host.length() >= sizeof(config::Settings::starlinkHost) || !ip.fromString(host)) {
        return fail("Dish address must be an IPv4 address like 192.168.100.1.");
    }

    config::Settings next = *_current;
    // Empty password on the same network keeps the saved one; on a
    // different network it means "open network".
    const bool keepPassword = pass.isEmpty() && ssid == _current->wifiSsid;
    strlcpy(next.wifiSsid, ssid.c_str(), sizeof(next.wifiSsid));
    if (!keepPassword) strlcpy(next.wifiPassword, pass.c_str(), sizeof(next.wifiPassword));
    strlcpy(next.starlinkHost, host.c_str(), sizeof(next.starlinkHost));

    LOG("PORTAL", "New settings: WiFi \"%s\"%s, Starlink %s", next.wifiSsid, keepPassword ? " (password kept)" : "",
        next.starlinkHost);
    sendJson(200, "{\"ok\":true}");
    _onSave(next);
}

void ConfigPortal::handleStatus() {
    const WifiStatus s = _status();
    String body = "{\"sta\":\"";
    body += staName(s.sta);
    body += "\",\"ssid\":";
    appendJsonString(body, s.ssid);
    body += ",\"ip\":\"";
    body += IPAddress(s.ip).toString();
    body += "\",\"failures\":";
    body += s.failures;
    body += ",\"error\":";
    appendJsonString(body, s.lastError);
    body += '}';
    sendJson(200, body);
}

void ConfigPortal::handleNotFound() {
    // Any unknown URL (including OS captive-portal probes such as
    // /generate_204 or /hotspot-detect.html) is redirected to the setup page.
    _server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
    _server.send(302, "text/plain", "");
}

}  // namespace net
