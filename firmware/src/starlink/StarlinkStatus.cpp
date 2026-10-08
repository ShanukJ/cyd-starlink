#include "StarlinkStatus.h"

namespace starlink {

uint8_t Alerts::count() const {
    const bool flags[] = {motorsStuck,        thermalShutdown,        thermalThrottle,   unexpectedLocation,
                          mastNotNearVertical, slowEthernetSpeeds,     slowEthernetSpeeds100,
                          noEthernetLink,      installPending,         isHeating,         powerSupplyThermalThrottle,
                          isPowerSaveIdle,     lowMotorCurrent,        lowerSignalThanPredicted,
                          obstructionMapReset, dishWaterDetected,      routerWaterDetected,
                          upsuRouterPortSlow,  dbfTelemStale};
    uint8_t n = unrecognized;
    for (bool f : flags) n += f;
    return n;
}

const char* attitudeStateName(AttitudeState s) {
    switch (s) {
        case AttitudeState::Reset: return "Reset";
        case AttitudeState::Unconverged: return "Converging";
        case AttitudeState::Converged: return "Converged";
        case AttitudeState::Faulted: return "Faulted";
        case AttitudeState::Invalid: return "Invalid";
        case AttitudeState::Unrecognized: break;
    }
    return "Unknown";
}

const char* softwareUpdateStateName(SoftwareUpdateState s) {
    switch (s) {
        case SoftwareUpdateState::Idle: return "Up to date";
        case SoftwareUpdateState::Fetching: return "Downloading";
        case SoftwareUpdateState::PreCheck: return "Checking";
        case SoftwareUpdateState::Writing: return "Installing";
        case SoftwareUpdateState::PostCheck: return "Verifying";
        case SoftwareUpdateState::RebootRequired: return "Reboot required";
        case SoftwareUpdateState::Disabled: return "Disabled";
        case SoftwareUpdateState::Faulted: return "Faulted";
        case SoftwareUpdateState::Unrecognized: break;
    }
    return "Unknown";
}

const char* disablementName(int32_t code) {
    switch (code) {
        case 1: return "In service";
        case 2: return "No active account";
        case 3: return "Too far from service address";
        case 4: return "In ocean";
        case 6: return "Blocked country";
        case 7: return "Data overage";
        case 8: return "Cell disabled";
        case 10: return "Roaming restricted";
        case 11: return "Unknown location";
        case 12: return "Account disabled";
        case 13: return "Unsupported version";
        case 14: return "Moving too fast";
        case 15: return "Aviation limits";
        case 16: return "Blocked area";
        case 17: return "Outside home region";
        default: return "Service disabled";
    }
}

const char* outageCauseName(int32_t cause) {
    switch (cause) {
        case 1: return "Booting";
        case 2: return "Stowed";
        case 3: return "Thermal shutdown";
        case 4: return "No schedule";
        case 5: return "No satellites";
        case 6: return "Obstructed";
        case 7: return "No downlink";
        case 8: return "No pings";
        case 9: return "Motors moving";
        case 10: return "Cable test";
        case 11: return "Sleeping";
        case 13: return "Searching sky";
        case 14: return "RF inhibited";
        default: return "Outage";
    }
}

}  // namespace starlink
