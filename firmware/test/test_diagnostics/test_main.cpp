// Host tests for starlink::evaluateChecks.

#include <unity.h>

#include "../fixtures/get_status_api43.h"
#include "starlink/Diagnostics.h"
#include "starlink/StatusDecoder.h"

using namespace starlink;

void setUp() {}
void tearDown() {}

static Check checks[kCheckCount];

static const Check& get(CheckId id) { return checks[static_cast<int>(id)]; }

#define ASSERT_CHECK(id, expectedLevel, expectedDetail)                                              \
    do {                                                                                             \
        TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(expectedLevel), static_cast<int>(get(id).level), \
                                      get(id).name);                                                 \
        TEST_ASSERT_EQUAL_STRING(expectedDetail, get(id).detail);                                    \
    } while (0)

static StarlinkStatus realStatus() {
    StarlinkStatus s;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecodeResult::Ok),
                          static_cast<int>(decodeGetStatus(kGetStatusApi43, kGetStatusApi43_len, s)));
    return s;
}

void test_real_dish_all_ok() {
    evaluateChecks(realStatus(), checks);
    ASSERT_CHECK(CheckId::Hardware, CheckLevel::Ok, "Self-test OK");  // cady=false must not fail it
    ASSERT_CHECK(CheckId::Rf, CheckLevel::Ok, "Ready");
    ASSERT_CHECK(CheckId::Gps, CheckLevel::Ok, "10 satellites");
    ASSERT_CHECK(CheckId::Network, CheckLevel::Ok, "Connected");
    ASSERT_CHECK(CheckId::Thermal, CheckLevel::Ok, "Normal");
    ASSERT_CHECK(CheckId::Obstruction, CheckLevel::Ok, "5.7% of sky");
    ASSERT_CHECK(CheckId::Ethernet, CheckLevel::Ok, "1000 Mbps");
    ASSERT_CHECK(CheckId::Alerts, CheckLevel::Ok, "None");
    ASSERT_CHECK(CheckId::Software, CheckLevel::Ok, "Up to date");
    TEST_ASSERT_EQUAL_STRING("OBSTRUCTION", get(CheckId::Obstruction).name);
}

void test_empty_status_is_unknown_not_ok() {
    evaluateChecks(StarlinkStatus{}, checks);
    for (int i = 0; i < kCheckCount; ++i) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(CheckLevel::Unknown), static_cast<int>(checks[i].level),
                                      checks[i].name);
        TEST_ASSERT_EQUAL_STRING("--", checks[i].detail);
    }
}

void test_failures() {
    StarlinkStatus s = realStatus();
    s.readyStates->xphy = false;
    s.readyStates->rf = false;
    s.gpsValid = false;
    s.outageActive = true;
    s.outageCause = 5;
    s.alerts->thermalShutdown = true;
    s.alerts->noEthernetLink = true;
    s.softwareUpdateState = SoftwareUpdateState::Faulted;
    evaluateChecks(s, checks);
    ASSERT_CHECK(CheckId::Hardware, CheckLevel::Fail, "XPHY not ready");
    ASSERT_CHECK(CheckId::Rf, CheckLevel::Fail, "Not ready");
    ASSERT_CHECK(CheckId::Gps, CheckLevel::Warn, "No fix");
    ASSERT_CHECK(CheckId::Network, CheckLevel::Fail, "No satellites");
    ASSERT_CHECK(CheckId::Thermal, CheckLevel::Fail, "Shutdown");
    ASSERT_CHECK(CheckId::Ethernet, CheckLevel::Fail, "No link");
    ASSERT_CHECK(CheckId::Software, CheckLevel::Fail, "Update failed");
}

void test_disablement_beats_outage() {
    StarlinkStatus s = realStatus();
    s.disablementCode = 2;
    s.outageActive = true;
    evaluateChecks(s, checks);
    ASSERT_CHECK(CheckId::Network, CheckLevel::Fail, "No active account");
}

void test_warnings_and_info() {
    StarlinkStatus s = realStatus();
    s.snrPersistentlyLow = true;
    s.currentlyObstructed = true;
    s.alerts->isHeating = true;
    s.alerts->slowEthernetSpeeds100 = true;
    s.softwareUpdateState = SoftwareUpdateState::Fetching;
    s.softwareUpdateProgress = 0.45f;
    evaluateChecks(s, checks);
    ASSERT_CHECK(CheckId::Rf, CheckLevel::Warn, "Low signal");
    ASSERT_CHECK(CheckId::Obstruction, CheckLevel::Warn, "Obstructed now");
    ASSERT_CHECK(CheckId::Thermal, CheckLevel::Info, "Heating");
    ASSERT_CHECK(CheckId::Ethernet, CheckLevel::Warn, "Slow (100 Mbps)");
    ASSERT_CHECK(CheckId::Software, CheckLevel::Info, "Downloading 45%");
}

void test_other_alerts_are_never_hidden() {
    StarlinkStatus s = realStatus();
    s.alerts->dishWaterDetected = true;
    evaluateChecks(s, checks);
    ASSERT_CHECK(CheckId::Alerts, CheckLevel::Warn, "Water in dish");

    s.alerts->mastNotNearVertical = true;
    s.alerts->unrecognized = 1;
    evaluateChecks(s, checks);
    ASSERT_CHECK(CheckId::Alerts, CheckLevel::Warn, "Water in dish +2");

    s = realStatus();
    s.alerts->unrecognized = 2;  // only alerts this firmware doesn't know
    evaluateChecks(s, checks);
    ASSERT_CHECK(CheckId::Alerts, CheckLevel::Warn, "2 new alerts");
}

void test_tiny_obstruction_is_not_zero() {
    StarlinkStatus s = realStatus();
    s.fractionObstructed = 0.0002f;
    evaluateChecks(s, checks);
    ASSERT_CHECK(CheckId::Obstruction, CheckLevel::Ok, "<0.1% of sky");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_real_dish_all_ok);
    RUN_TEST(test_empty_status_is_unknown_not_ok);
    RUN_TEST(test_failures);
    RUN_TEST(test_disablement_beats_outage);
    RUN_TEST(test_warnings_and_info);
    RUN_TEST(test_other_alerts_are_never_hidden);
    RUN_TEST(test_tiny_obstruction_is_not_zero);
    return UNITY_END();
}
