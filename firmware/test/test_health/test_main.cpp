// Host tests for starlink::evaluateHealth.

#include <string.h>
#include <unity.h>

#include "../fixtures/get_status_api43.h"
#include "starlink/Health.h"
#include "starlink/StarlinkSnapshot.h"
#include "starlink/StatusDecoder.h"

using namespace starlink;

void setUp() {}
void tearDown() {}

#define ASSERT_HEALTH(expectedState, expectedReason, health)                                     \
    do {                                                                                         \
        const Health h_ = (health);                                                              \
        TEST_ASSERT_EQUAL_STRING(healthStateName(expectedState), healthStateName(h_.state));     \
        TEST_ASSERT_EQUAL_STRING(expectedReason, h_.reason);                                     \
    } while (0)

// A snapshot built from the real (healthy) dish response.
static StarlinkSnapshot onlineSnapshot() {
    StarlinkSnapshot s;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecodeResult::Ok),
                          static_cast<int>(decodeGetStatus(kGetStatusApi43, kGetStatusApi43_len, s.status)));
    s.state = LinkState::Online;
    s.hasStatus = true;
    return s;
}

void test_link_states() {
    StarlinkSnapshot s;
    s.state = LinkState::Waiting;
    ASSERT_HEALTH(HealthState::Connecting, "Waiting for WiFi", evaluateHealth(s));
    s.state = LinkState::Connecting;
    ASSERT_HEALTH(HealthState::Connecting, "Contacting dish", evaluateHealth(s));
    s.state = LinkState::Unreachable;
    s.lastResult = ClientResult::TransportError;
    ASSERT_HEALTH(HealthState::Offline, "Dish unreachable", evaluateHealth(s));
    s.lastResult = ClientResult::NotADish;
    ASSERT_HEALTH(HealthState::Error, "Not a Starlink dish", evaluateHealth(s));
    s.lastResult = ClientResult::Malformed;
    ASSERT_HEALTH(HealthState::Error, "Unreadable dish data", evaluateHealth(s));
}

void test_real_dish_is_online() { ASSERT_HEALTH(HealthState::Online, "", evaluateHealth(onlineSnapshot())); }

void test_outage_is_offline_with_cause() {
    StarlinkSnapshot s = onlineSnapshot();
    s.status.outageActive = true;
    s.status.outageCause = 5;
    ASSERT_HEALTH(HealthState::Offline, "No satellites", evaluateHealth(s));
    s.status.outageCause.reset();
    ASSERT_HEALTH(HealthState::Offline, "Outage", evaluateHealth(s));
}

void test_disablement_is_error() {
    StarlinkSnapshot s = onlineSnapshot();
    s.status.disablementCode = 2;
    ASSERT_HEALTH(HealthState::Error, "No active account", evaluateHealth(s));
    s.status.disablementCode = 99;  // future code
    ASSERT_HEALTH(HealthState::Error, "Service disabled", evaluateHealth(s));
    s.status.disablementCode.reset();  // unknown is not an error
    ASSERT_HEALTH(HealthState::Online, "", evaluateHealth(s));
}

void test_alerts() {
    StarlinkSnapshot s = onlineSnapshot();
    s.status.alerts->thermalThrottle = true;
    ASSERT_HEALTH(HealthState::Degraded, "Thermal throttling", evaluateHealth(s));
    s.status.alerts->thermalShutdown = true;
    ASSERT_HEALTH(HealthState::Offline, "Thermal shutdown", evaluateHealth(s));

    s = onlineSnapshot();
    s.status.alerts->noEthernetLink = true;
    ASSERT_HEALTH(HealthState::Degraded, "Slow Ethernet", evaluateHealth(s));

    s = onlineSnapshot();
    s.status.alerts->isHeating = true;  // informational only
    ASSERT_HEALTH(HealthState::Online, "", evaluateHealth(s));
}

void test_obstruction_and_packet_loss() {
    StarlinkSnapshot s = onlineSnapshot();
    s.status.currentlyObstructed = true;
    ASSERT_HEALTH(HealthState::Degraded, "Obstructed", evaluateHealth(s));

    s = onlineSnapshot();
    s.status.popPingDropRate = 0.05f;
    ASSERT_HEALTH(HealthState::Online, "", evaluateHealth(s));
    s.status.popPingDropRate = kDegradedDropRate;
    ASSERT_HEALTH(HealthState::Degraded, "Packet loss", evaluateHealth(s));
}

void test_missing_data_does_not_degrade() {
    StarlinkSnapshot s;
    s.state = LinkState::Online;  // reachable, but every optional field empty
    ASSERT_HEALTH(HealthState::Online, "", evaluateHealth(s));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_link_states);
    RUN_TEST(test_real_dish_is_online);
    RUN_TEST(test_outage_is_offline_with_cause);
    RUN_TEST(test_disablement_is_error);
    RUN_TEST(test_alerts);
    RUN_TEST(test_obstruction_and_packet_loss);
    RUN_TEST(test_missing_data_does_not_degrade);
    return UNITY_END();
}
