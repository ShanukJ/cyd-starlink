#include "WifiManager.h"

#include <WiFi.h>
#include <esp_random.h>

#include "../utils/Log.h"

namespace net {

namespace {

constexpr const char* kHostname = "starlink-monitor";
constexpr uint32_t kConnectTimeoutMs = 20000;
constexpr uint32_t kMinBackoffMs = 2000;
constexpr uint32_t kMaxBackoffMs = 60000;
constexpr uint32_t kLinkLostRetryMs = 1000;
constexpr uint16_t kFailuresBeforePortal = 3;
constexpr uint32_t kPortalCloseAfterConnectMs = 15000;
constexpr uint32_t kPortalIdleCloseMs = 120000;
constexpr uint32_t kRssiPollMs = 2000;

// Written by the WiFi event task, read by the network task.
std::atomic<uint8_t> s_disconnectReason{0};
std::atomic<uint32_t> s_disconnectCount{0};
uint32_t s_attemptDisconnects = 0;  // s_disconnectCount at attempt start

// Per-boot setup AP credentials. The password is random and only shown
// on the device's own screen, so only someone looking at it can join.
char s_apSsid[33] = "";
char s_apPassword[9] = "";

void makeApCredentials() {
    if (s_apSsid[0]) return;
    const uint64_t mac = ESP.getEfuseMac();
    // Efuse MAC is little-endian in the uint64: bytes 4,5 are the last two.
    snprintf(s_apSsid, sizeof(s_apSsid), "STARLINK-MONITOR-%02X%02X", (uint8_t)(mac >> 32), (uint8_t)(mac >> 40));
    static const char kAlphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";  // no 0/o/1/l/i
    for (size_t i = 0; i < sizeof(s_apPassword) - 1; ++i) {
        s_apPassword[i] = kAlphabet[esp_random() % (sizeof(kAlphabet) - 1)];
    }
    s_apPassword[sizeof(s_apPassword) - 1] = '\0';
}

const char* reasonName(uint8_t reason) {
    return reason ? WiFi.STA.disconnectReasonName(static_cast<wifi_err_reason_t>(reason)) : "UNKNOWN";
}

bool reached(uint32_t now, uint32_t deadline) { return static_cast<int32_t>(now - deadline) >= 0; }

}  // namespace

void WifiManager::begin(const config::Settings& settings) {
    _settings = settings;

    WiFi.onEvent(
        [](arduino_event_id_t, arduino_event_info_t info) {
            s_disconnectReason = info.wifi_sta_disconnected.reason;
            s_disconnectCount++;
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.persistent(false);       // credentials live in our own NVS namespace
    WiFi.setAutoReconnect(false); // reconnects are paced by this class
    WiFi.setHostname(kHostname);
    WiFi.mode(WIFI_STA);
    // Mains-powered appliance: modem sleep only adds latency (measured
    // 40-330 ms per dish request with it on).
    WiFi.setSleep(false);

    _sta = _settings.hasWifi() ? StaState::WaitingRetry : StaState::NotConfigured;
    _nextAttempt = millis();
    publish();
}

WifiStatus WifiManager::status() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _status;
}

void WifiManager::loop() {
    step(millis());
    _portal.loop();
    publish();
}

void WifiManager::step(uint32_t now) {
    if (_portalRequested.exchange(false) && !_portal.active()) startPortal(now, "requested");
    if (_portalCloseRequested.exchange(false) && _portal.active() && _settings.hasWifi()) {
        stopPortal("closed on device");
    }

    switch (_sta) {
        case StaState::NotConfigured:
            if (!_portal.active()) startPortal(now, "no saved network");
            break;

        case StaState::WaitingRetry:
            if (!reached(now, _nextAttempt)) break;
            // A station connect attempt scans channels, which disrupts phones
            // on the setup AP. Hold off while someone is configuring, unless
            // they just asked us to connect.
            if (_portal.active() && _portalClients > 0 && !_savePending) {
                _nextAttempt = now + 5000;
                break;
            }
            startConnect(now);
            break;

        case StaState::Connecting: {
            const wl_status_t st = WiFi.status();
            if (st == WL_CONNECTED) {
                _sta = StaState::Connected;
                _failures = 0;
                _everConnected = true;
                _connectedAt = now;
                _lastError = "";
                _rssi = WiFi.RSSI();
                _lastRssiPoll = now;
                LOG("WIFI", "Connected: %s (RSSI %d dBm, channel %d)", WiFi.localIP().toString().c_str(), _rssi,
                    (int)WiFi.channel());
            } else if (s_disconnectCount != s_attemptDisconnects) {
                onConnectFailed(now, reasonName(s_disconnectReason));
            } else if (now - _attemptStart > kConnectTimeoutMs) {
                onConnectFailed(now, "TIMEOUT");
            }
            break;
        }

        case StaState::Connected:
            if (WiFi.status() != WL_CONNECTED) {
                _lastError = reasonName(s_disconnectReason);
                LOG("WIFI", "Disconnected (%s) - reconnecting", _lastError);
                _sta = StaState::WaitingRetry;
                _nextAttempt = now + kLinkLostRetryMs;
            } else if (now - _lastRssiPoll >= kRssiPollMs) {
                _rssi = WiFi.RSSI();
                _lastRssiPoll = now;
            }
            break;
    }

    if (!_everConnected && _failures >= kFailuresBeforePortal && !_portal.active()) {
        startPortal(now, "cannot join saved network");
    }

    if (_portal.active()) {
        _portalClients = WiFi.softAPgetStationNum();
        if (_portalClients > 0) _portalLastActivity = now;
        if (_sta == StaState::Connected) {
            if (_closePortalOnConnect && now - _connectedAt > kPortalCloseAfterConnectMs) {
                stopPortal("setup complete");
            } else if (!_closePortalOnConnect && now - _portalLastActivity > kPortalIdleCloseMs) {
                stopPortal("idle");
            }
        }
    }
}

void WifiManager::startConnect(uint32_t now) {
    if (_failures) {
        LOG("WIFI", "Connecting to \"%s\" (attempt %u)...", _settings.wifiSsid, _failures + 1);
    } else {
        LOG("WIFI", "Connecting to \"%s\"...", _settings.wifiSsid);
    }
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));  // let the disconnect event from the old link arrive first
    s_attemptDisconnects = s_disconnectCount;
    WiFi.begin(_settings.wifiSsid, _settings.wifiPassword[0] ? _settings.wifiPassword : nullptr);
    _attemptStart = now;
    _sta = StaState::Connecting;
    _savePending = false;
}

void WifiManager::onConnectFailed(uint32_t now, const char* why) {
    _failures++;
    _lastError = why;
    const uint32_t shift = _failures - 1 < 5 ? _failures - 1 : 5;
    uint32_t backoff = kMinBackoffMs << shift;
    if (backoff > kMaxBackoffMs) backoff = kMaxBackoffMs;
    WiFi.disconnect();
    _sta = StaState::WaitingRetry;
    _nextAttempt = now + backoff;
    LOG("WIFI", "Could not connect to \"%s\" (%s), retry in %lu s", _settings.wifiSsid, why,
        (unsigned long)(backoff / 1000));
}

void WifiManager::startPortal(uint32_t now, const char* why) {
    if (!reached(now, _portalRetryAt)) return;
    makeApCredentials();
    WiFi.mode(WIFI_AP_STA);
    if (!WiFi.softAP(s_apSsid, s_apPassword)) {
        LOG("WIFI", "ERROR: could not start setup AP, retrying in 5 s");
        _portalRetryAt = now + 5000;
        return;
    }
    strlcpy(_apIp, WiFi.softAPIP().toString().c_str(), sizeof(_apIp));
    _portal.start(
        &_settings, [this](const config::Settings& s) { applySettings(s); }, [this] { return status(); });
    _portalLastActivity = now;
    _closePortalOnConnect = false;
    LOG("WIFI", "Setup AP \"%s\" password \"%s\" at http://%s/ (%s)", s_apSsid, s_apPassword, _apIp, why);
}

void WifiManager::stopPortal(const char* why) {
    _portal.stop();
    _portalClients = 0;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _closePortalOnConnect = false;
    LOG("WIFI", "Setup AP closed (%s)", why);
}

void WifiManager::applySettings(const config::Settings& s) {
    _settings = s;
    config::save(_settings);
    _failures = 0;
    _lastError = "";
    _sta = StaState::WaitingRetry;
    _nextAttempt = millis();
    _savePending = true;
    _closePortalOnConnect = true;
    publish();  // so /status never reports the previous attempt's failure
}

void WifiManager::publish() {
    WifiStatus s;
    s.sta = _sta;
    strlcpy(s.ssid, _settings.wifiSsid, sizeof(s.ssid));
    if (_sta == StaState::Connected) {
        s.ip = static_cast<uint32_t>(WiFi.localIP());
        s.rssi = _rssi;
    }
    s.failures = _failures;
    if (_sta == StaState::WaitingRetry) {
        const uint32_t now = millis();
        s.retryInMs = reached(now, _nextAttempt) ? 0 : _nextAttempt - now;
    }
    s.lastError = _lastError;
    s.portalActive = _portal.active();
    if (s.portalActive) {
        strlcpy(s.apSsid, s_apSsid, sizeof(s.apSsid));
        strlcpy(s.apPassword, s_apPassword, sizeof(s.apPassword));
        strlcpy(s.apIp, _apIp, sizeof(s.apIp));
        s.portalClients = _portalClients;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    _status = s;
}

}  // namespace net
