#include "Health.h"

#include "StarlinkSnapshot.h"

namespace starlink {

const char* healthStateName(HealthState s) {
    switch (s) {
        case HealthState::Connecting: return "CONNECTING";
        case HealthState::Online: return "ONLINE";
        case HealthState::Degraded: return "DEGRADED";
        case HealthState::Offline: return "OFFLINE";
        case HealthState::Error: return "ERROR";
    }
    return "UNKNOWN";
}

Health evaluateHealth(const StarlinkSnapshot& s) {
    switch (s.state) {
        case LinkState::Waiting: return {HealthState::Connecting, "Waiting for WiFi"};
        case LinkState::Connecting: return {HealthState::Connecting, "Contacting dish"};
        case LinkState::Unreachable:
            switch (s.lastResult) {
                case ClientResult::ApiError: return {HealthState::Error, "Dish API error"};
                case ClientResult::NotADish: return {HealthState::Error, "Not a Starlink dish"};
                case ClientResult::Malformed: return {HealthState::Error, "Unreadable dish data"};
                default: return {HealthState::Offline, "Dish unreachable"};
            }
        case LinkState::Online: break;
    }

    const StarlinkStatus& st = s.status;

    // Service problems first: the dish works but won't carry traffic.
    if (st.disablementCode && *st.disablementCode != kDisablementOkay) {
        return {HealthState::Error, disablementName(*st.disablementCode)};
    }
    if (st.outageActive) {
        return {HealthState::Offline, st.outageCause ? outageCauseName(*st.outageCause) : "Outage"};
    }
    if (st.alerts && st.alerts->thermalShutdown) return {HealthState::Offline, "Thermal shutdown"};

    // Connected, but something needs attention.
    if (st.alerts) {
        const Alerts& a = *st.alerts;
        if (a.thermalThrottle || a.powerSupplyThermalThrottle) return {HealthState::Degraded, "Thermal throttling"};
        if (a.motor()) return {HealthState::Degraded, "Motor problem"};
        if (a.water()) return {HealthState::Degraded, "Water detected"};
        if (a.mastNotNearVertical) return {HealthState::Degraded, "Mast not vertical"};
        if (a.unexpectedLocation) return {HealthState::Degraded, "Unexpected location"};
        if (a.ethernet()) return {HealthState::Degraded, "Slow Ethernet"};
    }
    if (st.currentlyObstructed.value_or(false)) return {HealthState::Degraded, "Obstructed"};
    if (st.popPingDropRate && *st.popPingDropRate >= kDegradedDropRate) return {HealthState::Degraded, "Packet loss"};
    if (st.snrPersistentlyLow.value_or(false)) return {HealthState::Degraded, "Low signal"};

    return {HealthState::Online, ""};
}

}  // namespace starlink
