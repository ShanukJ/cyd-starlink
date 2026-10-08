// Host tests for starlink::decodeGetStatus.
//
// Run: pio test -e native
// Built with AddressSanitizer/UBSan: any out-of-bounds read fails the run.

#include <math.h>
#include <string.h>
#include <unity.h>

#include <string>

#include "../fixtures/get_status_api43.h"
#include "../support/ProtoWriter.h"
#include "starlink/StatusDecoder.h"

using namespace starlink;

void setUp() {}
void tearDown() {}

static DecodeResult decode(const ProtoWriter& w, StarlinkStatus& s, int32_t* apiErr = nullptr) {
    return decodeGetStatus(w.b.data(), w.b.size(), s, apiErr);
}

// Unity compares ints; enum classes need an explicit cast.
#define ASSERT_ENUM(expected, actual) TEST_ASSERT_EQUAL_INT(static_cast<int>(expected), static_cast<int>(actual))
#define ASSERT_ABSENT(opt) TEST_ASSERT_FALSE_MESSAGE((opt).has_value(), #opt " should be unavailable")
#define ASSERT_FLOAT(expected, opt)                                     \
    do {                                                                \
        TEST_ASSERT_TRUE_MESSAGE((opt).has_value(), #opt " missing");   \
        TEST_ASSERT_FLOAT_WITHIN(1e-4f * fabsf(expected) + 1e-6f, expected, *(opt)); \
    } while (0)

// --- Real dish response ------------------------------------------------------

// Expected values were decoded independently (Python) from the same bytes.
void test_real_response_decodes_every_field() {
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decodeGetStatus(kGetStatusApi43, kGetStatusApi43_len, s));

    TEST_ASSERT_EQUAL_UINT32(43, *s.apiVersion);
    TEST_ASSERT_EQUAL_STRING("rev4_panda_prod2", s.hardwareVersion);
    TEST_ASSERT_EQUAL_STRING("2026.09.18.mr87170.1.55184.2", s.softwareVersion);
    TEST_ASSERT_EQUAL_STRING("XX", s.countryCode);
    TEST_ASSERT_EQUAL_INT32(78, *s.bootCount);
    TEST_ASSERT_EQUAL_UINT64(98031, *s.uptimeS);

    ASSERT_FLOAT(46170.898f, s.downlinkBps);
    ASSERT_FLOAT(58141.441f, s.uplinkBps);
    ASSERT_FLOAT(35.719654f, s.popPingLatencyMs);
    ASSERT_FLOAT(0.0f, s.popPingDropRate);  // absent on the wire = 0 (proto3 default)
    ASSERT_FLOAT(0.58f, s.signalQuality);
    TEST_ASSERT_TRUE(*s.snrAboveNoiseFloor);
    TEST_ASSERT_FALSE(*s.snrPersistentlyLow);
    TEST_ASSERT_EQUAL_INT32(1000, *s.ethSpeedMbps);
    TEST_ASSERT_FALSE(s.outageActive);

    ASSERT_FLOAT(0.057319f, s.fractionObstructed);
    ASSERT_FLOAT(0.000275f, s.timeObstructed);
    ASSERT_FLOAT(97865.0f, s.obstructionValidS);
    TEST_ASSERT_FALSE(*s.currentlyObstructed);

    TEST_ASSERT_TRUE(*s.gpsValid);
    TEST_ASSERT_EQUAL_UINT32(10, *s.gpsSatellites);

    ASSERT_FLOAT(-11.408951f, s.azimuthDeg);
    ASSERT_FLOAT(71.252411f, s.elevationDeg);
    ASSERT_FLOAT(-0.044364f, s.desiredAzimuthDeg);
    ASSERT_FLOAT(76.017212f, s.desiredElevationDeg);
    ASSERT_FLOAT(18.004686f, s.tiltDeg);
    ASSERT_FLOAT(0.22568f, s.attitudeUncertaintyDeg);
    ASSERT_ENUM(AttitudeState::Converged, *s.attitudeState);

    TEST_ASSERT_EQUAL_INT32(kDisablementOkay, *s.disablementCode);
    ASSERT_ENUM(SoftwareUpdateState::Idle, *s.softwareUpdateState);
    ASSERT_FLOAT(1.0f, s.softwareUpdateProgress);

    TEST_ASSERT_TRUE(s.alerts.has_value());  // empty DishAlerts = no alerts
    TEST_ASSERT_EQUAL_UINT8(0, s.alerts->count());
    TEST_ASSERT_TRUE(s.readyStates.has_value());
    TEST_ASSERT_TRUE(s.readyStates->scp && s.readyStates->l1l2 && s.readyStates->xphy && s.readyStates->aap &&
                     s.readyStates->rf);
}

// --- Missing data ------------------------------------------------------------

void test_empty_dish_message() {
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(ProtoWriter{}), s));

    // Sub-messages absent => everything in them unavailable.
    ASSERT_ABSENT(s.uptimeS);
    ASSERT_ABSENT(s.fractionObstructed);
    ASSERT_ABSENT(s.currentlyObstructed);
    ASSERT_ABSENT(s.gpsValid);
    ASSERT_ABSENT(s.gpsSatellites);
    ASSERT_ABSENT(s.azimuthDeg);
    ASSERT_ABSENT(s.tiltDeg);
    ASSERT_ABSENT(s.attitudeState);
    ASSERT_ABSENT(s.alerts);
    ASSERT_ABSENT(s.readyStates);
    ASSERT_ABSENT(s.softwareUpdateState);
    TEST_ASSERT_EQUAL_STRING("", s.softwareVersion);

    // Zero-is-unknown fields stay unavailable.
    ASSERT_ABSENT(s.popPingLatencyMs);
    ASSERT_ABSENT(s.signalQuality);
    ASSERT_ABSENT(s.ethSpeedMbps);
    ASSERT_ABSENT(s.disablementCode);

    // Zero-is-value fields are real zeros.
    ASSERT_FLOAT(0.0f, s.downlinkBps);
    ASSERT_FLOAT(0.0f, s.uplinkBps);
    ASSERT_FLOAT(0.0f, s.popPingDropRate);
    TEST_ASSERT_FALSE(s.outageActive);
}

void test_empty_submessages_mean_defaults() {
    ProtoWriter dish, empty;
    dish.msg(2, empty).msg(1004, empty).msg(1015, empty).msg(1027, empty);
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    TEST_ASSERT_EQUAL_UINT64(0, *s.uptimeS);
    ASSERT_FLOAT(0.0f, s.fractionObstructed);
    TEST_ASSERT_FALSE(*s.currentlyObstructed);
    TEST_ASSERT_FALSE(*s.gpsValid);
    TEST_ASSERT_EQUAL_UINT32(0, *s.gpsSatellites);
    ASSERT_FLOAT(0.0f, s.tiltDeg);
    ASSERT_ENUM(AttitudeState::Reset, *s.attitudeState);  // enum 0 is a real state
}

void test_zero_latency_is_unavailable() {
    ProtoWriter dish;
    dish.f32(1009, 0.0f).u(1016, 0);
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    ASSERT_ABSENT(s.popPingLatencyMs);
    ASSERT_ABSENT(s.ethSpeedMbps);
}

// --- Bad values --------------------------------------------------------------

void test_nan_and_inf_are_unavailable() {
    ProtoWriter obstruction, dish;
    obstruction.f32(1, NAN).f32(9, 0.01f);
    dish.msg(1004, obstruction).f32(1009, INFINITY).f32(1007, -INFINITY);
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    ASSERT_ABSENT(s.fractionObstructed);
    ASSERT_FLOAT(0.01f, s.timeObstructed);
    ASSERT_ABSENT(s.popPingLatencyMs);
    ASSERT_ABSENT(s.downlinkBps);
}

void test_wrong_wire_type_is_unavailable_not_zero() {
    ProtoWriter dish;
    dish.u(1007, 5).str(1009, "fast").f32(1016, 100.0f);  // float as varint, etc.
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    ASSERT_ABSENT(s.downlinkBps);
    ASSERT_ABSENT(s.popPingLatencyMs);
    ASSERT_ABSENT(s.ethSpeedMbps);
}

void test_out_of_range_values_dropped() {
    ProtoWriter obstruction, align, dish;
    obstruction.f32(1, 1.5f);
    align.f32(5, 120.0f).f32(3, 18.0f);
    dish.msg(1004, obstruction).msg(1027, align).f32(1057, -0.1f).f32(1003, 2.0f);
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    ASSERT_ABSENT(s.fractionObstructed);
    ASSERT_ABSENT(s.elevationDeg);
    ASSERT_FLOAT(18.0f, s.tiltDeg);
    ASSERT_ABSENT(s.signalQuality);
    ASSERT_ABSENT(s.popPingDropRate);
}

// --- Schema evolution ------------------------------------------------------

void test_unknown_fields_ignored_everywhere() {
    ProtoWriter junk, gps, dish;
    junk.u(1, 7).str(2, "x");
    gps.u(1, 1).u(2, 12).u(99, 5).msg(100, junk);
    dish.msg(1015, gps).u(5000, 1).msg(7777, junk).f32(8888, 1.0f).f32(1009, 25.0f);
    ProtoWriter resp = dishResponse(dish);
    resp.u(9999, 3).msg(9998, junk);
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(resp, s));
    TEST_ASSERT_EQUAL_UINT32(12, *s.gpsSatellites);
    ASSERT_FLOAT(25.0f, s.popPingLatencyMs);
}

void test_unknown_alert_is_counted() {
    ProtoWriter alerts, dish;
    alerts.u(3, 1).u(99, 1).u(100, 0);
    dish.msg(1005, alerts);
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    TEST_ASSERT_TRUE(s.alerts->thermalThrottle);
    TEST_ASSERT_TRUE(s.alerts->thermal());
    TEST_ASSERT_EQUAL_UINT8(1, s.alerts->unrecognized);
    TEST_ASSERT_EQUAL_UINT8(2, s.alerts->count());
}

void test_unrecognized_enum_values() {
    ProtoWriter align, update, dish;
    align.u(6, 9);
    update.u(1, 42);
    dish.msg(1027, align).msg(1026, update);
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    ASSERT_ENUM(AttitudeState::Unrecognized, *s.attitudeState);
    ASSERT_ENUM(SoftwareUpdateState::Unrecognized, *s.softwareUpdateState);
}

void test_fallbacks_for_older_fields() {
    ProtoWriter dish;
    dish.f32(1011, 10.0f).f32(1012, 60.0f).u(1021, 6);  // no alignment_stats / update stats
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    ASSERT_FLOAT(10.0f, s.azimuthDeg);
    ASSERT_FLOAT(60.0f, s.elevationDeg);
    ASSERT_ABSENT(s.desiredAzimuthDeg);
    ASSERT_ENUM(SoftwareUpdateState::RebootRequired, *s.softwareUpdateState);
}

void test_outage() {
    ProtoWriter outage, dish;
    outage.u(1, 5);
    dish.msg(1014, outage);
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    TEST_ASSERT_TRUE(s.outageActive);
    TEST_ASSERT_EQUAL_INT32(5, *s.outageCause);

    ProtoWriter dish2;
    dish2.msg(1014, ProtoWriter{});  // cause 0 = UNKNOWN
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish2), s));
    TEST_ASSERT_TRUE(s.outageActive);
    ASSERT_ABSENT(s.outageCause);
}

void test_long_strings_truncated_safely() {
    char longVersion[200];
    memset(longVersion, 'v', sizeof(longVersion) - 1);
    longVersion[sizeof(longVersion) - 1] = '\0';
    ProtoWriter info, dish;
    info.str(3, longVersion).str(2, "");
    dish.msg(1, info);
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::Ok, decode(dishResponse(dish), s));
    TEST_ASSERT_EQUAL_size_t(sizeof(s.softwareVersion) - 1, strlen(s.softwareVersion));
    TEST_ASSERT_EQUAL_STRING("", s.hardwareVersion);
}

// --- Envelope ----------------------------------------------------------------

void test_api_error() {
    ProtoWriter status, resp;
    status.u(1, 7).str(2, "denied");
    resp.msg(2, status).u(3, 43);
    StarlinkStatus s;
    int32_t code = 0;
    ASSERT_ENUM(DecodeResult::ApiError, decode(resp, s, &code));
    TEST_ASSERT_EQUAL_INT32(7, code);
    ASSERT_ABSENT(s.apiVersion);  // no partial data on failure
}

void test_not_a_dish() {
    ProtoWriter resp;
    resp.u(3, 43).msg(2005, ProtoWriter{});  // some other device's status
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::NotADish, decode(resp, s));
}

void test_empty_input() {
    StarlinkStatus s;
    ASSERT_ENUM(DecodeResult::NotADish, decodeGetStatus(nullptr, 0, s));
}

// --- Corruption --------------------------------------------------------------

void test_every_truncation_fails_cleanly() {
    std::vector<uint8_t> buf(kGetStatusApi43, kGetStatusApi43 + kGetStatusApi43_len);
    for (size_t len = 0; len < buf.size(); ++len) {
        std::vector<uint8_t> cut(buf.begin(), buf.begin() + len);  // exact-size heap copy for ASan
        StarlinkStatus s;
        const DecodeResult r = decodeGetStatus(cut.data(), cut.size(), s);
        TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(static_cast<int>(DecodeResult::Ok), static_cast<int>(r),
                                          "a truncated response must not decode as OK");
        TEST_ASSERT_FALSE(s.downlinkBps.has_value());
    }
}

static uint32_t rng = 0x12345678;
static uint32_t nextRand() {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

static void assertSane(const StarlinkStatus& s) {
    auto finite = [](const std::optional<float>& v) { return !v || isfinite(*v); };
    TEST_ASSERT_TRUE(finite(s.downlinkBps) && finite(s.uplinkBps) && finite(s.popPingLatencyMs) &&
                     finite(s.azimuthDeg) && finite(s.elevationDeg) && finite(s.tiltDeg));
    if (s.fractionObstructed) TEST_ASSERT_TRUE(*s.fractionObstructed >= 0 && *s.fractionObstructed <= 1);
    if (s.signalQuality) TEST_ASSERT_TRUE(*s.signalQuality >= 0 && *s.signalQuality <= 1);
    TEST_ASSERT_TRUE(strlen(s.softwareVersion) < sizeof(s.softwareVersion));
}

void test_random_mutations_never_crash() {
    int ok = 0;
    for (int i = 0; i < 20000; ++i) {
        std::vector<uint8_t> buf(kGetStatusApi43, kGetStatusApi43 + kGetStatusApi43_len);
        const int edits = 1 + nextRand() % 8;
        for (int e = 0; e < edits; ++e) {
            const size_t at = nextRand() % buf.size();
            switch (nextRand() % 4) {
                case 0: buf[at] ^= static_cast<uint8_t>(1u << (nextRand() % 8)); break;
                case 1: buf[at] = static_cast<uint8_t>(nextRand()); break;
                case 2: buf.erase(buf.begin() + at); break;
                case 3: buf.insert(buf.begin() + at, static_cast<uint8_t>(nextRand())); break;
            }
            if (buf.empty()) buf.push_back(0);
        }
        StarlinkStatus s;
        if (decodeGetStatus(buf.data(), buf.size(), s) == DecodeResult::Ok) {
            ok++;
            assertSane(s);
        }
    }
    TEST_MESSAGE((std::string("mutations decoded as OK: ") + std::to_string(ok) + " / 20000").c_str());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_real_response_decodes_every_field);
    RUN_TEST(test_empty_dish_message);
    RUN_TEST(test_empty_submessages_mean_defaults);
    RUN_TEST(test_zero_latency_is_unavailable);
    RUN_TEST(test_nan_and_inf_are_unavailable);
    RUN_TEST(test_wrong_wire_type_is_unavailable_not_zero);
    RUN_TEST(test_out_of_range_values_dropped);
    RUN_TEST(test_unknown_fields_ignored_everywhere);
    RUN_TEST(test_unknown_alert_is_counted);
    RUN_TEST(test_unrecognized_enum_values);
    RUN_TEST(test_fallbacks_for_older_fields);
    RUN_TEST(test_outage);
    RUN_TEST(test_long_strings_truncated_safely);
    RUN_TEST(test_api_error);
    RUN_TEST(test_not_a_dish);
    RUN_TEST(test_empty_input);
    RUN_TEST(test_every_truncation_fails_cleanly);
    RUN_TEST(test_random_mutations_never_crash);
    return UNITY_END();
}
