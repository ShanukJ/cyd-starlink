// Host tests for ui::fmt.

#include <math.h>
#include <unity.h>

#include "ui/Format.h"

using namespace ui;

void setUp() {}
void tearDown() {}

static char buf[32];

static void checkThroughput(std::optional<float> bps, const char* value, const char* unit) {
    const char* u = nullptr;
    fmt::throughput(bps, buf, sizeof(buf), &u);
    TEST_ASSERT_EQUAL_STRING(value, buf);
    TEST_ASSERT_EQUAL_STRING(unit, u);
}

void test_throughput() {
    checkThroughput(std::nullopt, "--", "Mbps");
    checkThroughput(0.0f, "0", "kbps");  // a real zero stays a zero
    checkThroughput(46170.9f, "46", "kbps");
    checkThroughput(999400.0f, "999", "kbps");
    checkThroughput(1.0e6f, "1.0", "Mbps");
    checkThroughput(4.64e6f, "4.6", "Mbps");
    checkThroughput(9.96e6f, "10", "Mbps");
    checkThroughput(183.4e6f, "183", "Mbps");
}

void test_latency() {
    fmt::latency(std::nullopt, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("--", buf);
    fmt::latency(42.4f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("42", buf);
}

void test_percent() {
    fmt::percent(std::nullopt, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("--", buf);
    fmt::percent(0.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("0.0", buf);
    fmt::percent(0.000275f, buf, sizeof(buf));  // 0.0275 %: not "0.0"
    TEST_ASSERT_EQUAL_STRING("<0.1", buf);
    fmt::percent(0.0573f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("5.7", buf);
    fmt::percent(0.58f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("58", buf);
    fmt::percent(1.0f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("100", buf);
}

void test_duration() {
    fmt::duration(std::optional<uint64_t>(), buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("--", buf);
    fmt::duration(42, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("42s", buf);
    fmt::duration(312, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("5m 12s", buf);
    fmt::duration(98031, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("1d 3h", buf);
    fmt::duration(13 * 3600 + 5 * 60, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("13h 05m", buf);
    fmt::duration(4 * 86400 + 13 * 3600 + 59 * 60, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("4d 13h", buf);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_throughput);
    RUN_TEST(test_latency);
    RUN_TEST(test_percent);
    RUN_TEST(test_duration);
    return UNITY_END();
}
