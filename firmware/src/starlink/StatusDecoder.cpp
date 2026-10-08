#include "StatusDecoder.h"

#include <math.h>

#include "ProtoReader.h"

// Turning proto3 into "available / unavailable"
// ----------------------------------------------
// Proto3 does not send fields that hold their default value (0, false, "").
// So an absent field can mean "zero" or "this firmware doesn't send it".
// Every field is therefore resolved with one of two policies, and only when
// its parent message is present (absent parent => everything unavailable):
//
//   ZeroIsValue    absent => default. Used where 0/false is a normal reading
//                  of a long-standing field (throughput, obstruction, flags).
//   ZeroIsUnknown  absent or 0 => unavailable. Used where 0 is not a real
//                  reading (latency, Ethernet speed, enum "UNKNOWN" values)
//                  or the field is recent enough that older dishes omit it
//                  (signal_quality).
//
// In addition, for every field: a wrong wire type (schema change) or a
// NaN/Inf float makes it unavailable, and values outside a physically
// plausible range are dropped. Unknown fields are skipped.
//
// Field numbers: dish gRPC reflection, api_version 43 (2026-10). See
// docs/protocol/starlink-grpc-web.md.

namespace starlink {

const char* decodeResultName(DecodeResult r) {
    switch (r) {
        case DecodeResult::Ok: return "OK";
        case DecodeResult::ApiError: return "API_ERROR";
        case DecodeResult::NotADish: return "NOT_A_DISH";
        case DecodeResult::Malformed: return "MALFORMED";
    }
    return "UNKNOWN";
}

namespace {

enum class Zero : uint8_t { IsValue, IsUnknown };

// One scalar field as seen on the wire, before policy is applied.
template <typename T>
struct Slot {
    enum : uint8_t { Absent, Valid, Invalid } state = Absent;
    T value{};

    void set(T v) {
        state = Valid;
        value = v;
    }
    void invalidate() { state = Invalid; }
    bool present() const { return state == Valid; }

    std::optional<T> resolve(Zero zero) const {
        switch (state) {
            case Absent: return zero == Zero::IsValue ? std::optional<T>(T{}) : std::nullopt;
            case Invalid: return std::nullopt;
            case Valid: break;
        }
        if (zero == Zero::IsUnknown && value == T{}) return std::nullopt;
        return value;
    }
};

// Readers that refuse wire-type mismatches and non-finite floats.
void take(Slot<float>& s, const ProtoReader& r) {
    if (r.wireType() != ProtoReader::Fixed32 || !isfinite(r.float32())) return s.invalidate();
    s.set(r.float32());
}
void take(Slot<bool>& s, const ProtoReader& r) {
    if (r.wireType() != ProtoReader::Varint) return s.invalidate();
    s.set(r.boolean());
}
void take(Slot<int32_t>& s, const ProtoReader& r) {
    if (r.wireType() != ProtoReader::Varint) return s.invalidate();
    s.set(r.int32());
}
void take(Slot<uint32_t>& s, const ProtoReader& r) {
    if (r.wireType() != ProtoReader::Varint) return s.invalidate();
    s.set(static_cast<uint32_t>(r.varint()));
}
void take(Slot<uint64_t>& s, const ProtoReader& r) {
    if (r.wireType() != ProtoReader::Varint) return s.invalidate();
    s.set(r.varint());
}
void takeString(char* out, size_t size, const ProtoReader& r) {
    if (r.wireType() == ProtoReader::Length) r.copyString(out, size);
}

// Drops values outside [lo, hi].
void clampToRange(std::optional<float>& v, float lo, float hi) {
    if (v && (*v < lo || *v > hi)) v.reset();
}

// --- Field numbers ---------------------------------------------------------

namespace F {
// SpaceX.API.Device.Response
constexpr uint32_t kRespStatus = 2, kRespApiVersion = 3, kRespDishGetStatus = 2004;
// SpaceX.API.Status.Status
constexpr uint32_t kStatusCode = 1;
// SpaceX.API.Device.DishGetStatusResponse
constexpr uint32_t kDeviceInfo = 1, kDeviceState = 2, kPopPingDropRate = 1003, kObstructionStats = 1004,
                   kAlerts = 1005, kDownlinkBps = 1007, kUplinkBps = 1008, kPopPingLatencyMs = 1009,
                   kBoresightAzimuth = 1011, kBoresightElevation = 1012, kOutage = 1014, kGpsStats = 1015,
                   kEthSpeedMbps = 1016, kSnrAboveNoiseFloor = 1018, kReadyStates = 1019,
                   kSoftwareUpdateState = 1021, kSnrPersistentlyLow = 1022, kDisablementCode = 1024,
                   kSoftwareUpdateStats = 1026, kAlignmentStats = 1027, kSignalQuality = 1057;
// DeviceInfo
constexpr uint32_t kHardwareVersion = 2, kSoftwareVersion = 3, kCountryCode = 4, kBootcount = 8;
// DeviceState
constexpr uint32_t kUptimeS = 1;
// DishObstructionStats
constexpr uint32_t kFractionObstructed = 1, kValidS = 4, kCurrentlyObstructed = 5, kTimeObstructed = 9;
// DishOutage
constexpr uint32_t kOutageCause = 1;
// DishGpsStats
constexpr uint32_t kGpsValid = 1, kGpsSats = 2;
// AlignmentStats
constexpr uint32_t kTilt = 3, kAlignAzimuth = 4, kAlignElevation = 5, kAttitudeState = 6, kAttitudeUncertainty = 7,
                   kDesiredAzimuth = 8, kDesiredElevation = 9;
// SoftwareUpdateStats
constexpr uint32_t kUpdateState = 1, kUpdateProgress = 2;
}  // namespace F

// --- Sub-message decoders ----------------------------------------------------

bool decodeDeviceInfo(ProtoReader r, StarlinkStatus& out) {
    Slot<int32_t> bootcount;
    while (r.next()) {
        switch (r.field()) {
            case F::kHardwareVersion: takeString(out.hardwareVersion, sizeof(out.hardwareVersion), r); break;
            case F::kSoftwareVersion: takeString(out.softwareVersion, sizeof(out.softwareVersion), r); break;
            case F::kCountryCode: takeString(out.countryCode, sizeof(out.countryCode), r); break;
            case F::kBootcount: take(bootcount, r); break;
        }
    }
    out.bootCount = bootcount.resolve(Zero::IsUnknown);
    return r.ok();
}

bool decodeDeviceState(ProtoReader r, StarlinkStatus& out) {
    Slot<uint64_t> uptime;
    while (r.next()) {
        if (r.field() == F::kUptimeS) take(uptime, r);
    }
    out.uptimeS = uptime.resolve(Zero::IsValue);
    return r.ok();
}

bool decodeObstruction(ProtoReader r, StarlinkStatus& out) {
    Slot<float> fraction, validS, timeObstructed;
    Slot<bool> currently;
    while (r.next()) {
        switch (r.field()) {
            case F::kFractionObstructed: take(fraction, r); break;
            case F::kValidS: take(validS, r); break;
            case F::kCurrentlyObstructed: take(currently, r); break;
            case F::kTimeObstructed: take(timeObstructed, r); break;
        }
    }
    out.fractionObstructed = fraction.resolve(Zero::IsValue);
    out.timeObstructed = timeObstructed.resolve(Zero::IsValue);
    out.obstructionValidS = validS.resolve(Zero::IsValue);
    out.currentlyObstructed = currently.resolve(Zero::IsValue);
    clampToRange(out.fractionObstructed, 0.0f, 1.0f);
    clampToRange(out.timeObstructed, 0.0f, 1.0f);
    clampToRange(out.obstructionValidS, 0.0f, 1e9f);
    return r.ok();
}

bool decodeAlerts(ProtoReader r, StarlinkStatus& out) {
    Alerts a;
    while (r.next()) {
        if (r.wireType() != ProtoReader::Varint) continue;
        const bool on = r.boolean();
        switch (r.field()) {
            case 1: a.motorsStuck = on; break;
            case 2: a.thermalShutdown = on; break;
            case 3: a.thermalThrottle = on; break;
            case 4: a.unexpectedLocation = on; break;
            case 5: a.mastNotNearVertical = on; break;
            case 6: a.slowEthernetSpeeds = on; break;
            case 8: a.installPending = on; break;
            case 9: a.isHeating = on; break;
            case 10: a.powerSupplyThermalThrottle = on; break;
            case 11: a.isPowerSaveIdle = on; break;
            case 14: a.dbfTelemStale = on; break;
            case 16: a.lowMotorCurrent = on; break;
            case 17: a.lowerSignalThanPredicted = on; break;
            case 18: a.slowEthernetSpeeds100 = on; break;
            case 19: a.obstructionMapReset = on; break;
            case 20: a.dishWaterDetected = on; break;
            case 21: a.routerWaterDetected = on; break;
            case 22: a.upsuRouterPortSlow = on; break;
            case 23: a.noEthernetLink = on; break;
            default:
                // A newer alert this firmware doesn't know: still count it so
                // the UI can say "1 other alert" instead of hiding it.
                if (on && a.unrecognized < 255) a.unrecognized++;
                break;
        }
    }
    out.alerts = a;
    return r.ok();
}

bool decodeOutage(ProtoReader r, StarlinkStatus& out) {
    Slot<int32_t> cause;
    while (r.next()) {
        if (r.field() == F::kOutageCause) take(cause, r);
    }
    out.outageActive = true;
    out.outageCause = cause.resolve(Zero::IsUnknown);  // 0 = UNKNOWN
    return r.ok();
}

bool decodeGps(ProtoReader r, StarlinkStatus& out) {
    Slot<bool> valid;
    Slot<uint32_t> sats;
    while (r.next()) {
        switch (r.field()) {
            case F::kGpsValid: take(valid, r); break;
            case F::kGpsSats: take(sats, r); break;
        }
    }
    out.gpsValid = valid.resolve(Zero::IsValue);
    out.gpsSatellites = sats.resolve(Zero::IsValue);
    if (out.gpsSatellites && *out.gpsSatellites > 200) out.gpsSatellites.reset();
    return r.ok();
}

bool decodeReadyStates(ProtoReader r, StarlinkStatus& out) {
    ReadyStates s;
    while (r.next()) {
        if (r.wireType() != ProtoReader::Varint) continue;
        const bool on = r.boolean();
        switch (r.field()) {
            case 1: s.cady = on; break;
            case 2: s.scp = on; break;
            case 3: s.l1l2 = on; break;
            case 4: s.xphy = on; break;
            case 5: s.aap = on; break;
            case 6: s.rf = on; break;
        }
    }
    out.readyStates = s;
    return r.ok();
}

AttitudeState toAttitude(int32_t v) {
    return (v >= 0 && v <= 4) ? static_cast<AttitudeState>(v) : AttitudeState::Unrecognized;
}

SoftwareUpdateState toUpdateState(int32_t v) {
    return (v >= 1 && v <= 8) ? static_cast<SoftwareUpdateState>(v) : SoftwareUpdateState::Unrecognized;
}

bool decodeAlignment(ProtoReader r, StarlinkStatus& out) {
    Slot<float> tilt, az, el, uncertainty, desiredAz, desiredEl;
    Slot<int32_t> attitude;
    while (r.next()) {
        switch (r.field()) {
            case F::kTilt: take(tilt, r); break;
            case F::kAlignAzimuth: take(az, r); break;
            case F::kAlignElevation: take(el, r); break;
            case F::kAttitudeState: take(attitude, r); break;
            case F::kAttitudeUncertainty: take(uncertainty, r); break;
            case F::kDesiredAzimuth: take(desiredAz, r); break;
            case F::kDesiredElevation: take(desiredEl, r); break;
        }
    }
    out.tiltDeg = tilt.resolve(Zero::IsValue);
    out.azimuthDeg = az.resolve(Zero::IsValue);
    out.elevationDeg = el.resolve(Zero::IsValue);
    out.attitudeUncertaintyDeg = uncertainty.resolve(Zero::IsValue);
    out.desiredAzimuthDeg = desiredAz.resolve(Zero::IsValue);
    out.desiredElevationDeg = desiredEl.resolve(Zero::IsValue);
    // 0 = FILTER_RESET is a real state, not "unknown".
    if (auto a = attitude.resolve(Zero::IsValue)) out.attitudeState = toAttitude(*a);
    return r.ok();
}

bool decodeUpdateStats(ProtoReader r, StarlinkStatus& out) {
    Slot<int32_t> state;
    Slot<float> progress;
    while (r.next()) {
        switch (r.field()) {
            case F::kUpdateState: take(state, r); break;
            case F::kUpdateProgress: take(progress, r); break;
        }
    }
    if (auto s = state.resolve(Zero::IsUnknown)) out.softwareUpdateState = toUpdateState(*s);
    out.softwareUpdateProgress = progress.resolve(Zero::IsValue);
    clampToRange(out.softwareUpdateProgress, 0.0f, 1.0f);
    return r.ok();
}

bool decodeDish(ProtoReader r, StarlinkStatus& out) {
    Slot<float> drop, down, up, latency, boresightAz, boresightEl, signal;
    Slot<int32_t> eth, disablement, updateState;
    Slot<bool> snrAbove, snrLow;
    bool ok = true;

    while (ok && r.next()) {
        switch (r.field()) {
            case F::kDeviceInfo: ok = decodeDeviceInfo(r.message(), out); break;
            case F::kDeviceState: ok = decodeDeviceState(r.message(), out); break;
            case F::kObstructionStats: ok = decodeObstruction(r.message(), out); break;
            case F::kAlerts: ok = decodeAlerts(r.message(), out); break;
            case F::kOutage: ok = decodeOutage(r.message(), out); break;
            case F::kGpsStats: ok = decodeGps(r.message(), out); break;
            case F::kReadyStates: ok = decodeReadyStates(r.message(), out); break;
            case F::kAlignmentStats: ok = decodeAlignment(r.message(), out); break;
            case F::kSoftwareUpdateStats: ok = decodeUpdateStats(r.message(), out); break;
            case F::kPopPingDropRate: take(drop, r); break;
            case F::kDownlinkBps: take(down, r); break;
            case F::kUplinkBps: take(up, r); break;
            case F::kPopPingLatencyMs: take(latency, r); break;
            case F::kBoresightAzimuth: take(boresightAz, r); break;
            case F::kBoresightElevation: take(boresightEl, r); break;
            case F::kEthSpeedMbps: take(eth, r); break;
            case F::kSnrAboveNoiseFloor: take(snrAbove, r); break;
            case F::kSoftwareUpdateState: take(updateState, r); break;
            case F::kSnrPersistentlyLow: take(snrLow, r); break;
            case F::kDisablementCode: take(disablement, r); break;
            case F::kSignalQuality: take(signal, r); break;
            default: break;
        }
    }
    if (!ok || !r.ok()) return false;

    out.popPingDropRate = drop.resolve(Zero::IsValue);
    out.downlinkBps = down.resolve(Zero::IsValue);
    out.uplinkBps = up.resolve(Zero::IsValue);
    out.popPingLatencyMs = latency.resolve(Zero::IsUnknown);  // 0 => no successful pings
    out.signalQuality = signal.resolve(Zero::IsUnknown);       // recent field: absent on older dishes
    out.snrAboveNoiseFloor = snrAbove.resolve(Zero::IsValue);
    out.snrPersistentlyLow = snrLow.resolve(Zero::IsValue);
    out.ethSpeedMbps = eth.resolve(Zero::IsUnknown);            // 0 => no link / unknown
    out.disablementCode = disablement.resolve(Zero::IsUnknown); // 0 = UNKNOWN_STATE

    clampToRange(out.popPingDropRate, 0.0f, 1.0f);
    clampToRange(out.downlinkBps, 0.0f, 1e11f);
    clampToRange(out.uplinkBps, 0.0f, 1e11f);
    clampToRange(out.popPingLatencyMs, 0.0f, 60000.0f);
    clampToRange(out.signalQuality, 0.0f, 1.0f);
    if (out.ethSpeedMbps && (*out.ethSpeedMbps < 0 || *out.ethSpeedMbps > 100000)) out.ethSpeedMbps.reset();

    // Pointing: prefer alignment_stats; fall back to the older top-level
    // boresight fields only if the dish actually sent them (a fallback must
    // never turn "absent" into 0).
    if (!out.azimuthDeg && boresightAz.present()) out.azimuthDeg = boresightAz.value;
    if (!out.elevationDeg && boresightEl.present()) out.elevationDeg = boresightEl.value;
    clampToRange(out.azimuthDeg, -360.0f, 360.0f);
    clampToRange(out.desiredAzimuthDeg, -360.0f, 360.0f);
    clampToRange(out.elevationDeg, -90.0f, 90.0f);
    clampToRange(out.desiredElevationDeg, -90.0f, 90.0f);
    clampToRange(out.tiltDeg, 0.0f, 90.0f);
    clampToRange(out.attitudeUncertaintyDeg, 0.0f, 360.0f);

    // Software update state: prefer software_update_stats, fall back to the
    // top-level enum.
    if (!out.softwareUpdateState) {
        if (auto s = updateState.resolve(Zero::IsUnknown)) out.softwareUpdateState = toUpdateState(*s);
    }
    return true;
}

}  // namespace

DecodeResult decodeGetStatus(const uint8_t* data, size_t len, StarlinkStatus& out, int32_t* apiErrorCode) {
    out = StarlinkStatus{};
    int32_t apiError = 0;
    Slot<uint32_t> apiVersion;
    bool haveDish = false;
    bool ok = true;

    ProtoReader r(data, len);
    while (ok && r.next()) {
        switch (r.field()) {
            case F::kRespStatus: {
                ProtoReader s = r.message();
                while (s.next()) {
                    if (s.field() == F::kStatusCode && s.wireType() == ProtoReader::Varint) apiError = s.int32();
                }
                ok = s.ok();
                break;
            }
            case F::kRespApiVersion: take(apiVersion, r); break;
            case F::kRespDishGetStatus:
                if (r.wireType() != ProtoReader::Length) {
                    ok = false;
                    break;
                }
                haveDish = true;
                ok = decodeDish(r.message(), out);
                break;
            default: break;
        }
    }
    if (apiErrorCode) *apiErrorCode = apiError;

    DecodeResult result = DecodeResult::Ok;
    if (!ok || !r.ok()) {
        result = DecodeResult::Malformed;
    } else if (apiError != 0) {
        result = DecodeResult::ApiError;
    } else if (!haveDish) {
        result = DecodeResult::NotADish;
    }
    if (result != DecodeResult::Ok) {
        out = StarlinkStatus{};  // never hand out partial telemetry
        return result;
    }
    out.apiVersion = apiVersion.resolve(Zero::IsUnknown);
    return DecodeResult::Ok;
}

}  // namespace starlink
