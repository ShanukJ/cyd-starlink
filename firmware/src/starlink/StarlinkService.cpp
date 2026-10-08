#include "StarlinkService.h"

#include <Arduino.h>
#include <string.h>

#include "../utils/Log.h"

namespace starlink {

namespace {

constexpr uint32_t kPollIntervalMs = 2000;
constexpr uint32_t kMinBackoffMs = 2000;
constexpr uint32_t kMaxBackoffMs = 10000;  // keep it short: the dish reboots in ~1 min
constexpr uint32_t kSummaryIntervalMs = 5 * 60 * 1000;

bool reached(uint32_t now, uint32_t t) { return static_cast<int32_t>(now - t) >= 0; }

// Short human-readable reason for logs and the UI.
const char* errorName(ClientResult r, CallResult call) {
    if (r != ClientResult::TransportError) return clientResultName(r);
    return callResultName(call);
}

// Formats optional telemetry for logs; unavailable values print as "--"
// so the log never shows a fake zero. Holds a few buffers per log line.
class Fmt {
public:
    const char* f(const std::optional<float>& v, const char* fmt, float scale = 1.0f) {
        if (!v) return "--";
        snprintf(next(), kLen, fmt, *v * scale);
        return _buf[_i];
    }
    template <typename T>
    const char* n(const std::optional<T>& v) {
        if (!v) return "--";
        snprintf(next(), kLen, "%lld", static_cast<long long>(*v));
        return _buf[_i];
    }
    static const char* yesNo(const std::optional<bool>& v, const char* yes = "yes", const char* no = "no") {
        return v ? (*v ? yes : no) : "--";
    }

private:
    static constexpr size_t kLen = 16;
    char* next() {
        _i = (_i + 1) % 6;
        return _buf[_i];
    }
    char _buf[6][kLen];
    int _i = 0;
};

void logStatus(const StarlinkStatus& s) {
    Fmt x;
    const uint64_t up = s.uptimeS.value_or(0);
    LOG("STARLINK", "Dish %s, software %s, uptime %luh %02lum%s, boots %s",
        s.hardwareVersion[0] ? s.hardwareVersion : "--", s.softwareVersion[0] ? s.softwareVersion : "--",
        (unsigned long)(up / 3600), (unsigned long)(up / 60 % 60), s.uptimeS ? "" : " (unknown)", x.n(s.bootCount));
    LOG("STARLINK", "Link: down %s Mbps, up %s Mbps, latency %s ms, drop %s %%, signal %s %%, eth %s Mbps",
        x.f(s.downlinkBps, "%.2f", 1e-6f), x.f(s.uplinkBps, "%.2f", 1e-6f), x.f(s.popPingLatencyMs, "%.1f"),
        x.f(s.popPingDropRate, "%.1f", 100.0f), x.f(s.signalQuality, "%.0f", 100.0f), x.n(s.ethSpeedMbps));
    LOG("STARLINK", "Obstruction: %s %% of sky, %s %% of time, now %s; GPS %s, %s sats",
        x.f(s.fractionObstructed, "%.2f", 100.0f), x.f(s.timeObstructed, "%.3f", 100.0f),
        Fmt::yesNo(s.currentlyObstructed), Fmt::yesNo(s.gpsValid, "valid", "invalid"), x.n(s.gpsSatellites));
    LOG("STARLINK", "Pointing: az %s (want %s), el %s (want %s), tilt %s, attitude %s +/- %s deg",
        x.f(s.azimuthDeg, "%.1f"), x.f(s.desiredAzimuthDeg, "%.1f"), x.f(s.elevationDeg, "%.1f"),
        x.f(s.desiredElevationDeg, "%.1f"), x.f(s.tiltDeg, "%.1f"),
        s.attitudeState ? attitudeStateName(*s.attitudeState) : "--", x.f(s.attitudeUncertaintyDeg, "%.2f"));
    const ReadyStates* rs = s.readyStates ? &*s.readyStates : nullptr;
    LOG("STARLINK", "Service: %s, update %s, outage %s, alerts %s, self-test %s",
        s.disablementCode ? disablementName(*s.disablementCode) : "--",
        s.softwareUpdateState ? softwareUpdateStateName(*s.softwareUpdateState) : "--",
        s.outageActive ? (s.outageCause ? outageCauseName(*s.outageCause) : "yes") : "none",
        s.alerts ? x.n(std::optional<int>(s.alerts->count())) : "--",
        !rs ? "--" : (rs->scp && rs->l1l2 && rs->xphy && rs->aap && rs->rf) ? "all ready" : "NOT all ready");
}

}  // namespace

void StarlinkService::loop(uint32_t now, bool networkUp, const char* host) {
    if (strcmp(host, _host) != 0) {
        if (_host[0]) LOG("STARLINK", "Dish address changed to %s", host);
        strlcpy(_host, host, sizeof(_host));
        _failures = 0;
        _nextPoll = now;
        if (_state != LinkState::Waiting) _state = LinkState::Connecting;
    }

    if (!networkUp) {
        if (_state != LinkState::Waiting) {
            _state = LinkState::Waiting;
            publish();
        }
        return;
    }
    if (_state == LinkState::Waiting) {
        _state = LinkState::Connecting;
        _failures = 0;
        _nextPoll = now;
        LOG("STARLINK", "Connecting to %s:%u", _host, GrpcWebTransport::kDefaultPort);
        publish();
    }
    if (reached(now, _nextPoll)) poll(now);
}

void StarlinkService::poll(uint32_t now) {
    StarlinkStatus status;
    const ClientResult r = _client.getStatus(_host, status);
    const CallInfo& info = _client.callInfo();
    now = millis();  // the call took time

    if (r == ClientResult::Ok) {
        if (_state != LinkState::Online) {
            LOG("STARLINK", "API reachable (api %lu, %lu ms)", (unsigned long)status.apiVersion.value_or(0),
                (unsigned long)info.elapsedMs);
            logStatus(status);
            _lastSummaryMs = now;
            _pollsSinceSummary = 0;
        }
        _state = LinkState::Online;
        _failures = 0;
        _lastError = "";
        _lastResult = r;
        _last = status;
        _hasStatus = true;
        _lastOkMs = now;
        _lastRttMs = info.elapsedMs;
        _nextPoll = now + kPollIntervalMs;
        _pollsSinceSummary++;
        if (now - _lastSummaryMs >= kSummaryIntervalMs) {
            LOG("STARLINK", "OK: %lu polls in the last %lu min, last %lu ms", (unsigned long)_pollsSinceSummary,
                (unsigned long)(kSummaryIntervalMs / 60000), (unsigned long)info.elapsedMs);
            _lastSummaryMs = now;
            _pollsSinceSummary = 0;
        }
    } else {
        const char* why = errorName(r, _client.transportResult());
        const bool changed = why != _lastError || _state == LinkState::Online;
        _failures++;
        _lastError = why;
        _lastResult = r;
        const uint32_t shift = _failures - 1 < 3 ? _failures - 1 : 3;
        uint32_t backoff = kMinBackoffMs << shift;
        if (backoff > kMaxBackoffMs) backoff = kMaxBackoffMs;
        _nextPoll = now + backoff;
        if (changed) {
            if (r == ClientResult::TransportError && info.grpcStatus > 0) {
                LOG("STARLINK", "Request failed: %s (grpc-status %d: %s) - retrying", why, info.grpcStatus,
                    info.grpcMessage);
            } else if (r == ClientResult::ApiError) {
                LOG("STARLINK", "Request failed: API error code %ld - retrying", (long)_client.apiErrorCode());
            } else {
                LOG("STARLINK", "%s %s after %lu ms (%s) - retrying with backoff",
                    _state == LinkState::Online ? "Lost contact:" : "Unreachable:", _host,
                    (unsigned long)info.elapsedMs, why);
            }
        }
        if (_state == LinkState::Online || _failures >= 2) _state = LinkState::Unreachable;
    }
    publish();
}

void StarlinkService::publish() {
    StarlinkSnapshot snap;
    snap.state = _state;
    snap.hasStatus = _hasStatus;
    snap.status = _last;
    snap.lastOkMs = _lastOkMs;
    snap.lastRttMs = _lastRttMs;
    snap.failures = _failures;
    snap.lastResult = _lastResult;
    snap.lastError = _lastError;
    strlcpy(snap.host, _host, sizeof(snap.host));
    snap.health = evaluateHealth(snap);

    const Health& h = snap.health;
    if (h.state != _health.state || strcmp(h.reason, _health.reason) != 0) {
        LOG("STARLINK", "Health: %s%s%s", healthStateName(h.state), h.reason[0] ? " - " : "", h.reason);
        _health = h;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    _snapshot = snap;
}

StarlinkSnapshot StarlinkService::snapshot() const {
    std::lock_guard<std::mutex> lock(_mutex);
    return _snapshot;
}

}  // namespace starlink
