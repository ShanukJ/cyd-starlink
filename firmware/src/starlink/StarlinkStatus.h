#pragma once

#include <stdint.h>

#include <optional>

namespace starlink {

// Internal Starlink status model, independent of the wire format. The UI
// only ever sees this (plus Health, see Health.h).
//
// Rules:
//  - Unavailable telemetry is an empty std::optional (strings: ""), never 0.
//  - Values are in the units named by the field; fractions are 0..1.
//  - The dish's device ID is deliberately not stored (privacy).
// How "missing" is decided per field is documented in StatusDecoder.cpp.

enum class AttitudeState : uint8_t {  // SpaceX.API.Device.AttitudeEstimationState
    Reset = 0,
    Unconverged = 1,
    Converged = 2,
    Faulted = 3,
    Invalid = 4,
    Unrecognized = 255,  // a value this firmware doesn't know
};

enum class SoftwareUpdateState : uint8_t {  // SpaceX.API.Device.SoftwareUpdateState
    Idle = 1,
    Fetching = 2,
    PreCheck = 3,
    Writing = 4,
    PostCheck = 5,
    RebootRequired = 6,
    Disabled = 7,
    Faulted = 8,
    Unrecognized = 255,
};

// SpaceX.API.Satellites.Network.UtDisablementCode is open-ended (values are
// added over time), so it is kept as the raw code. 1 = OKAY.
constexpr int32_t kDisablementOkay = 1;

struct Alerts {  // SpaceX.API.Device.DishAlerts
    bool motorsStuck = false;
    bool thermalShutdown = false;
    bool thermalThrottle = false;
    bool unexpectedLocation = false;
    bool mastNotNearVertical = false;
    bool slowEthernetSpeeds = false;
    bool slowEthernetSpeeds100 = false;
    bool noEthernetLink = false;
    bool installPending = false;
    bool isHeating = false;
    bool powerSupplyThermalThrottle = false;
    bool isPowerSaveIdle = false;
    bool lowMotorCurrent = false;
    bool lowerSignalThanPredicted = false;
    bool obstructionMapReset = false;
    bool dishWaterDetected = false;
    bool routerWaterDetected = false;
    bool upsuRouterPortSlow = false;
    bool dbfTelemStale = false;
    uint8_t unrecognized = 0;  // active alerts added in newer dish firmware

    bool thermal() const { return thermalShutdown || thermalThrottle || powerSupplyThermalThrottle; }
    bool motor() const { return motorsStuck || lowMotorCurrent; }
    bool ethernet() const { return slowEthernetSpeeds || slowEthernetSpeeds100 || noEthernetLink || upsuRouterPortSlow; }
    bool water() const { return dishWaterDetected || routerWaterDetected; }
    uint8_t count() const;
};

struct ReadyStates {  // SpaceX.API.Device.DishReadyStates — subsystem self-checks
    bool cady = false;
    bool scp = false;
    bool l1l2 = false;
    bool xphy = false;
    bool aap = false;
    bool rf = false;
};

struct StarlinkStatus {
    std::optional<uint32_t> apiVersion;

    // Identity
    char hardwareVersion[32] = "";
    char softwareVersion[64] = "";
    char countryCode[4] = "";
    std::optional<int32_t> bootCount;
    std::optional<uint64_t> uptimeS;

    // Link quality
    std::optional<float> downlinkBps;
    std::optional<float> uplinkBps;
    std::optional<float> popPingLatencyMs;
    std::optional<float> popPingDropRate;  // 0..1
    std::optional<float> signalQuality;    // 0..1
    std::optional<bool> snrAboveNoiseFloor;
    std::optional<bool> snrPersistentlyLow;
    std::optional<int32_t> ethSpeedMbps;

    // Outage: the dish reports an outage message only while it is not
    // connected to the network. Valid whenever the status itself is valid.
    bool outageActive = false;
    std::optional<int32_t> outageCause;  // raw SpaceX.API.Device.DishOutage.Cause

    // Obstruction
    std::optional<float> fractionObstructed;  // 0..1, share of sky obstructed
    std::optional<float> timeObstructed;      // 0..1, as reported (time_obstructed)
    std::optional<float> obstructionValidS;   // seconds of data behind the stats
    std::optional<bool> currentlyObstructed;

    // GPS
    std::optional<bool> gpsValid;
    std::optional<uint32_t> gpsSatellites;

    // Alignment (degrees)
    std::optional<float> azimuthDeg;
    std::optional<float> elevationDeg;
    std::optional<float> desiredAzimuthDeg;
    std::optional<float> desiredElevationDeg;
    std::optional<float> tiltDeg;
    std::optional<float> attitudeUncertaintyDeg;
    std::optional<AttitudeState> attitudeState;

    // Service and software
    std::optional<int32_t> disablementCode;  // kDisablementOkay = in service
    std::optional<SoftwareUpdateState> softwareUpdateState;
    std::optional<float> softwareUpdateProgress;  // 0..1

    std::optional<Alerts> alerts;
    std::optional<ReadyStates> readyStates;
};

const char* attitudeStateName(AttitudeState s);
const char* softwareUpdateStateName(SoftwareUpdateState s);
const char* disablementName(int32_t code);   // human-readable
const char* outageCauseName(int32_t cause);  // human-readable

}  // namespace starlink
