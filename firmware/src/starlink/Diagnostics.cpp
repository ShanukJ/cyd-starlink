#include "Diagnostics.h"

#include <stdio.h>
#include <string.h>

namespace starlink {

namespace {

void set(Check& c, CheckLevel level, const char* detail) {
    c.level = level;
    snprintf(c.detail, sizeof(c.detail), "%s", detail);
}

void hardware(const StarlinkStatus& s, Check& c) {
    // Subsystem self-tests. `cady` is deliberately not checked: the
    // reference dish (rev4) never reports it ready while working normally.
    if (!s.readyStates) return;
    const ReadyStates& r = *s.readyStates;
    const char* failed = !r.scp ? "SCP" : !r.l1l2 ? "L1L2" : !r.xphy ? "XPHY" : !r.aap ? "AAP" : nullptr;
    if (failed) {
        char buf[sizeof(c.detail)];
        snprintf(buf, sizeof(buf), "%s not ready", failed);
        return set(c, CheckLevel::Fail, buf);
    }
    if (s.alerts && s.alerts->dbfTelemStale) return set(c, CheckLevel::Warn, "Telemetry stale");
    set(c, CheckLevel::Ok, "Self-test OK");
}

void rf(const StarlinkStatus& s, Check& c) {
    if (s.readyStates && !s.readyStates->rf) return set(c, CheckLevel::Fail, "Not ready");
    if (s.snrPersistentlyLow.value_or(false)) return set(c, CheckLevel::Warn, "Low signal");
    if (s.alerts && s.alerts->lowerSignalThanPredicted) return set(c, CheckLevel::Warn, "Signal below expected");
    if (s.snrAboveNoiseFloor && !*s.snrAboveNoiseFloor) return set(c, CheckLevel::Warn, "Below noise floor");
    if (s.readyStates || s.snrAboveNoiseFloor) set(c, CheckLevel::Ok, "Ready");
}

void gps(const StarlinkStatus& s, Check& c) {
    if (!s.gpsValid) return;
    char buf[sizeof(c.detail)];
    if (!*s.gpsValid) {
        return set(c, CheckLevel::Warn, "No fix");
    }
    if (s.gpsSatellites) {
        snprintf(buf, sizeof(buf), "%lu satellites", static_cast<unsigned long>(*s.gpsSatellites));
    } else {
        snprintf(buf, sizeof(buf), "Valid");
    }
    set(c, CheckLevel::Ok, buf);
}

void network(const StarlinkStatus& s, Check& c) {
    if (s.disablementCode && *s.disablementCode != kDisablementOkay) {
        return set(c, CheckLevel::Fail, disablementName(*s.disablementCode));
    }
    if (s.outageActive) {
        return set(c, CheckLevel::Fail, s.outageCause ? outageCauseName(*s.outageCause) : "Outage");
    }
    if (s.alerts && s.alerts->isPowerSaveIdle) return set(c, CheckLevel::Info, "Power save");
    if (s.disablementCode) set(c, CheckLevel::Ok, "Connected");
}

void thermal(const StarlinkStatus& s, Check& c) {
    if (!s.alerts) return;
    const Alerts& a = *s.alerts;
    if (a.thermalShutdown) return set(c, CheckLevel::Fail, "Shutdown");
    if (a.thermalThrottle) return set(c, CheckLevel::Warn, "Throttling");
    if (a.powerSupplyThermalThrottle) return set(c, CheckLevel::Warn, "PSU throttling");
    if (a.isHeating) return set(c, CheckLevel::Info, "Heating");
    set(c, CheckLevel::Ok, "Normal");
}

void obstruction(const StarlinkStatus& s, Check& c) {
    if (s.currentlyObstructed.value_or(false)) return set(c, CheckLevel::Warn, "Obstructed now");
    if (s.alerts && s.alerts->obstructionMapReset) return set(c, CheckLevel::Info, "Map reset");
    if (!s.fractionObstructed) return;
    // No judgement on the percentage itself: Starlink publishes no threshold.
    char buf[sizeof(c.detail)];
    const float p = *s.fractionObstructed * 100.0f;
    if (p > 0.0f && p < 0.05f) {
        snprintf(buf, sizeof(buf), "<0.1%% of sky");
    } else {
        snprintf(buf, sizeof(buf), "%.1f%% of sky", p);
    }
    set(c, CheckLevel::Ok, buf);
}

void ethernet(const StarlinkStatus& s, Check& c) {
    if (s.alerts) {
        const Alerts& a = *s.alerts;
        if (a.noEthernetLink) return set(c, CheckLevel::Fail, "No link");
        if (a.slowEthernetSpeeds100) return set(c, CheckLevel::Warn, "Slow (100 Mbps)");
        if (a.slowEthernetSpeeds) return set(c, CheckLevel::Warn, "Slow");
        if (a.upsuRouterPortSlow) return set(c, CheckLevel::Warn, "Router port slow");
    }
    if (!s.ethSpeedMbps) return;
    char buf[sizeof(c.detail)];
    snprintf(buf, sizeof(buf), "%ld Mbps", static_cast<long>(*s.ethSpeedMbps));
    set(c, CheckLevel::Ok, buf);
}

void otherAlerts(const StarlinkStatus& s, Check& c) {
    if (!s.alerts) return;
    const Alerts& a = *s.alerts;
    const char* names[8];
    int n = 0;
    if (a.motorsStuck) names[n++] = "Motors stuck";
    if (a.lowMotorCurrent) names[n++] = "Low motor current";
    if (a.dishWaterDetected) names[n++] = "Water in dish";
    if (a.routerWaterDetected) names[n++] = "Water in router";
    if (a.mastNotNearVertical) names[n++] = "Mast not vertical";
    if (a.unexpectedLocation) names[n++] = "Unexpected location";
    const int total = n + a.unrecognized;
    if (total == 0) return set(c, CheckLevel::Ok, "None");
    char buf[sizeof(c.detail)];
    if (n == 0) {
        snprintf(buf, sizeof(buf), "%d new alert%s", total, total > 1 ? "s" : "");
    } else if (total == 1) {
        snprintf(buf, sizeof(buf), "%s", names[0]);
    } else {
        snprintf(buf, sizeof(buf), "%s +%d", names[0], total - 1);
    }
    set(c, CheckLevel::Warn, buf);
}

void software(const StarlinkStatus& s, Check& c) {
    if (!s.softwareUpdateState) {
        if (s.alerts && s.alerts->installPending) set(c, CheckLevel::Info, "Install pending");
        return;
    }
    char buf[sizeof(c.detail)];
    switch (*s.softwareUpdateState) {
        case SoftwareUpdateState::Idle:
            if (s.alerts && s.alerts->installPending) return set(c, CheckLevel::Info, "Install pending");
            return set(c, CheckLevel::Ok, "Up to date");
        case SoftwareUpdateState::Fetching:
        case SoftwareUpdateState::Writing:
            if (s.softwareUpdateProgress) {
                snprintf(buf, sizeof(buf), "%s %.0f%%", softwareUpdateStateName(*s.softwareUpdateState),
                         *s.softwareUpdateProgress * 100.0f);
                return set(c, CheckLevel::Info, buf);
            }
            return set(c, CheckLevel::Info, softwareUpdateStateName(*s.softwareUpdateState));
        case SoftwareUpdateState::PreCheck:
        case SoftwareUpdateState::PostCheck:
            return set(c, CheckLevel::Info, softwareUpdateStateName(*s.softwareUpdateState));
        case SoftwareUpdateState::RebootRequired: return set(c, CheckLevel::Warn, "Reboot required");
        case SoftwareUpdateState::Disabled: return set(c, CheckLevel::Info, "Updates disabled");
        case SoftwareUpdateState::Faulted: return set(c, CheckLevel::Fail, "Update failed");
        case SoftwareUpdateState::Unrecognized: break;
    }
    set(c, CheckLevel::Unknown, "Unknown state");
}

}  // namespace

void evaluateChecks(const StarlinkStatus& s, Check out[kCheckCount]) {
    static const char* const kNames[kCheckCount] = {"HARDWARE", "RF",       "GPS",    "NETWORK", "THERMAL",
                                                    "OBSTRUCTION", "ETHERNET", "ALERTS", "SOFTWARE"};
    for (int i = 0; i < kCheckCount; ++i) {
        out[i] = Check{};
        out[i].name = kNames[i];
    }
    hardware(s, out[static_cast<int>(CheckId::Hardware)]);
    rf(s, out[static_cast<int>(CheckId::Rf)]);
    gps(s, out[static_cast<int>(CheckId::Gps)]);
    network(s, out[static_cast<int>(CheckId::Network)]);
    thermal(s, out[static_cast<int>(CheckId::Thermal)]);
    obstruction(s, out[static_cast<int>(CheckId::Obstruction)]);
    ethernet(s, out[static_cast<int>(CheckId::Ethernet)]);
    otherAlerts(s, out[static_cast<int>(CheckId::Alerts)]);
    software(s, out[static_cast<int>(CheckId::Software)]);
}

}  // namespace starlink
