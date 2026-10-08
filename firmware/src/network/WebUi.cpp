#include "WebUi.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include <algorithm>

#include "../app/Version.h"
#include "../config/SettingsValidation.h"
#include "../utils/JsonWriter.h"
#include "../utils/Log.h"
#include "WebPage.h"

namespace net {

namespace {

constexpr uint16_t kDnsPort = 53;
constexpr const char* kCsrfHeader = "X-SM-Request";
constexpr uint32_t kActionDelayMs = 800;  // let the HTTP response go out first

const char* staName(StaState s) {
    switch (s) {
        case StaState::NotConfigured: return "unconfigured";
        case StaState::Connecting: return "connecting";
        case StaState::Connected: return "connected";
        case StaState::WaitingRetry: return "retry";
    }
    return "unknown";
}

const char* linkName(starlink::LinkState s) {
    switch (s) {
        case starlink::LinkState::Waiting: return "waiting";
        case starlink::LinkState::Connecting: return "connecting";
        case starlink::LinkState::Online: return "online";
        case starlink::LinkState::Unreachable: return "unreachable";
    }
    return "unknown";
}

}  // namespace

void WebUi::begin(const Deps& deps) {
    _deps = deps;
    installRoutes();
    static const char* headers[] = {kCsrfHeader};
    _server.collectHeaders(headers, 1);
    _server.begin();
    LOG("WEB", "Web UI listening on port 80");
}

void WebUi::loop() {
    // Captive-portal DNS only while the setup AP is up.
    const bool portal = _deps.wifi->portalActive();
    if (portal && !_dnsRunning) {
        _dns.setErrorReplyCode(DNSReplyCode::NoError);
        _dnsRunning = _dns.start(kDnsPort, "*", WiFi.softAPIP());
        _scanDone = false;
        WiFi.scanNetworks(true);  // results ready when the phone opens the page
    } else if (!portal && _dnsRunning) {
        _dns.stop();
        _dnsRunning = false;
    }
    if (_dnsRunning) _dns.processNextRequest();

    // http://starlink-monitor.local/ once the station is on the LAN.
    if (!_mdnsStarted && WiFi.status() == WL_CONNECTED) {
        if (MDNS.begin(WifiManager::kHostname)) {
            MDNS.addService("http", "tcp", 80);
            _mdnsStarted = true;
            LOG("WEB", "Settings at http://%s.local/ or http://%s/", WifiManager::kHostname,
                WiFi.localIP().toString().c_str());
        }
    }

    _server.handleClient();

    if (_pending != Pending::None && static_cast<int32_t>(millis() - _pendingAt) >= 0) {
        if (_pending == Pending::FactoryReset) {
            LOG("WEB", "Factory reset requested");
            _deps.settings->factoryReset();
        }
        LOG("WEB", "Restarting");
        delay(100);
        ESP.restart();
    }
}

void WebUi::installRoutes() {
    _server.on("/", HTTP_GET, [this] { handleRoot(); });
    _server.on("/api/status", HTTP_GET, [this] { handleStatus(); });
    _server.on("/api/settings", HTTP_GET, [this] { handleGetSettings(); });
    _server.on("/api/settings", HTTP_POST, [this] { handlePostSettings(); });
    _server.on("/api/scan", HTTP_GET, [this] { handleScan(); });
    _server.on("/api/reboot", HTTP_POST, [this] { handleReboot(); });
    _server.on("/api/factory-reset", HTTP_POST, [this] { handleFactoryReset(); });
    _server.onNotFound([this] { handleNotFound(); });
}

// DNS-rebinding guard: a malicious site can point its own name at this
// device's IP, making its scripts "same-origin". Only answer the API when
// the browser addressed the device by one of its real names/addresses.
bool WebUi::apiAllowed() {
    String host = _server.hostHeader();
    const int colon = host.indexOf(':');
    if (colon >= 0) host.remove(colon);
    host.toLowerCase();
    const String mdnsName = String(WifiManager::kHostname);
    if (host == mdnsName || host == mdnsName + ".local") return true;
    if (WiFi.status() == WL_CONNECTED && host == WiFi.localIP().toString()) return true;
    if (_deps.wifi->portalActive() && host == _deps.wifi->apIp()) return true;
    sendError(403, "Open the monitor by its IP address or http://starlink-monitor.local/");
    return false;
}

// CSRF guard: a custom header can't be added to a cross-site request
// without a CORS preflight, and this server never approves one.
bool WebUi::writeAllowed() {
    if (!apiAllowed()) return false;
    if (!_server.hasHeader(kCsrfHeader)) {
        sendError(403, "Missing request header");
        return false;
    }
    return true;
}

void WebUi::sendJson(int code, const char* body) {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(code, "application/json", body);
}

void WebUi::sendError(int code, const char* message) {
    JsonWriter j(_json, sizeof(_json));
    j.beginObject().boolean("ok", false).str("error", message).endObject();
    sendJson(code, _json);
}

void WebUi::handleRoot() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send_P(200, "text/html", kWebPage, sizeof(kWebPage) - 1);
}

void WebUi::handleStatus() {
    if (!apiAllowed()) return;
    const WifiStatus w = _deps.wifi->status();
    const starlink::StarlinkSnapshot d = _deps.starlink->snapshot();
    const starlink::StarlinkStatus& st = d.status;
    const bool fresh = d.state == starlink::LinkState::Online && d.hasStatus;

    JsonWriter j(_json, sizeof(_json));
    j.beginObject();
    j.beginObject("monitor")
        .str("version", SM_VERSION)
        .str("build", SM_BUILD_DATE)
        .str("board", _deps.boardName)
        .integer("uptime_s", millis() / 1000)
        .integer("heap", ESP.getFreeHeap())
        .endObject();
    j.beginObject("wifi")
        .str("state", staName(w.sta))
        .str("ssid", w.ssid)
        .str("ip", w.sta == StaState::Connected ? IPAddress(w.ip).toString().c_str() : "")
        .integer("rssi", w.rssi)
        .integer("failures", w.failures)
        .str("error", w.lastError)
        .boolean("portal", w.portalActive)
        .endObject();
    j.beginObject("dish")
        .str("link", linkName(d.state))
        .str("health", starlink::healthStateName(d.health.state))
        .str("reason", d.health.reason)
        .str("host", d.host);
    if (d.hasStatus) {
        j.integer("last_seen_s", (millis() - d.lastOkMs) / 1000);
    } else {
        j.null("last_seen_s");
    }
    // Telemetry only while fresh; null (never 0) when unavailable.
    const starlink::StarlinkStatus empty;
    const starlink::StarlinkStatus& t = fresh ? st : empty;
    j.num("down_bps", t.downlinkBps)
        .num("up_bps", t.uplinkBps)
        .num("latency_ms", t.popPingLatencyMs, 1)
        .num("obstruction", t.fractionObstructed, 5)
        .num("signal", t.signalQuality, 3);
    if (t.uptimeS) {
        j.integer("uptime_s", static_cast<long long>(*t.uptimeS));
    } else {
        j.null("uptime_s");
    }
    j.str("software", st.softwareVersion).str("hardware", st.hardwareVersion);
    j.endObject().endObject();
    if (!j.ok()) return sendError(500, "Response too large");
    sendJson(200, _json);
}

void WebUi::handleGetSettings() {
    if (!apiAllowed()) return;
    const config::Settings s = _deps.settings->get();
    JsonWriter j(_json, sizeof(_json));
    j.beginObject()
        .str("ssid", s.wifiSsid)
        .boolean("has_password", s.wifiPassword[0] != '\0')  // never the password itself
        .str("host", s.starlinkHost)
        .integer("poll_ms", s.pollMs)
        .integer("brightness", s.brightness)
        .integer("rotation", s.rotation == config::kRotationBoardDefault ? _deps.boardRotation : s.rotation)
        .endObject();
    sendJson(200, _json);
}

void WebUi::handlePostSettings() {
    if (!writeAllowed()) return;

    // Keep the Strings alive while SettingsUpdate points into them.
    const String ssid = _server.arg("ssid"), pass = _server.arg("pass"), host = _server.arg("host"),
                 poll = _server.arg("poll_ms"), bright = _server.arg("brightness"), rot = _server.arg("rotation");
    config::SettingsUpdate u;
    if (_server.hasArg("ssid")) {
        u.ssid = ssid.c_str();
        u.password = pass.c_str();
    }
    if (_server.hasArg("host")) u.starlinkHost = host.c_str();
    if (_server.hasArg("poll_ms")) u.pollMs = poll.c_str();
    if (_server.hasArg("brightness")) u.brightness = bright.c_str();
    if (_server.hasArg("rotation")) u.rotation = rot.c_str();

    config::Settings next;
    const config::UpdateResult r = config::applyUpdate(_deps.settings->get(), u, next);
    if (!r.ok) return sendError(400, r.error);
    const bool saved = _deps.settings->update(next);
    LOG("WEB", "Settings updated%s%s", r.wifiChanged ? " (WiFi changed)" : "", saved ? "" : " - NOT persisted");

    JsonWriter j(_json, sizeof(_json));
    j.beginObject().boolean("ok", true).boolean("saved", saved).boolean("wifi_changed", r.wifiChanged).endObject();
    sendJson(200, _json);
}

void WebUi::collectScanResults(int16_t count) {
    _networkCount = 0;
    for (int16_t i = 0; i < count; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;  // hidden network
        const int8_t rssi = WiFi.RSSI(i);
        Network* existing = nullptr;  // de-duplicate (mesh): keep the strongest
        for (size_t k = 0; k < _networkCount; ++k) {
            if (ssid == _networks[k].ssid) existing = &_networks[k];
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

void WebUi::handleScan() {
    if (!apiAllowed()) return;
    if (_server.hasArg("refresh")) {
        _scanDone = false;
        WiFi.scanDelete();
        WiFi.scanNetworks(true);
    }
    if (!_scanDone) {
        const int16_t n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) return sendJson(200, "{\"state\":\"scanning\"}");
        if (n < 0) {
            // Failed or never started (e.g. the station was mid-connect).
            WiFi.scanNetworks(true);
            return sendJson(200, "{\"state\":\"scanning\"}");
        }
        collectScanResults(n);
    }
    JsonWriter j(_json, sizeof(_json));
    j.beginObject().str("state", "done").beginArray("networks");
    for (size_t i = 0; i < _networkCount; ++i) {
        j.beginObject().str("ssid", _networks[i].ssid).integer("rssi", _networks[i].rssi).boolean("open", _networks[i].open).endObject();
    }
    j.endArray().endObject();
    if (!j.ok()) return sendError(500, "Response too large");
    sendJson(200, _json);
}

void WebUi::handleReboot() {
    if (!writeAllowed()) return;
    sendJson(200, "{\"ok\":true}");
    _pending = Pending::Reboot;
    _pendingAt = millis() + kActionDelayMs;
}

void WebUi::handleFactoryReset() {
    if (!writeAllowed()) return;
    sendJson(200, "{\"ok\":true}");
    _pending = Pending::FactoryReset;
    _pendingAt = millis() + kActionDelayMs;
}

void WebUi::handleNotFound() {
    if (_deps.wifi->portalActive()) {
        // Captive portal: any unknown URL (including OS connectivity probes
        // such as /generate_204 or /hotspot-detect.html) goes to the page.
        _server.sendHeader("Location", String("http://") + _deps.wifi->apIp() + "/", true);
        _server.send(302, "text/plain", "");
        return;
    }
    _server.send(404, "text/plain", "Not found");
}

}  // namespace net
