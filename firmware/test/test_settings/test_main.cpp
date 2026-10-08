// Host tests for settings validation and the JSON writer used by the web UI.

#include <math.h>
#include <string.h>
#include <unity.h>

#include "config/SettingsValidation.h"
#include "utils/JsonWriter.h"

using namespace config;

void setUp() {}
void tearDown() {}

static Settings saved() {
    Settings s;
    strcpy(s.wifiSsid, "Home");
    strcpy(s.wifiPassword, "secret123");
    return s;
}

void test_ipv4() {
    TEST_ASSERT_TRUE(isValidIpv4("192.168.100.1"));
    TEST_ASSERT_TRUE(isValidIpv4("0.0.0.0"));
    TEST_ASSERT_TRUE(isValidIpv4("255.255.255.255"));
    TEST_ASSERT_FALSE(isValidIpv4("256.1.1.1"));
    TEST_ASSERT_FALSE(isValidIpv4("1.2.3"));
    TEST_ASSERT_FALSE(isValidIpv4("1.2.3.4.5"));
    TEST_ASSERT_FALSE(isValidIpv4("1.2.3.4 "));
    TEST_ASSERT_FALSE(isValidIpv4("01.2.3.4"));
    TEST_ASSERT_FALSE(isValidIpv4("dishy.starlink.com"));
    TEST_ASSERT_FALSE(isValidIpv4(""));
    TEST_ASSERT_FALSE(isValidIpv4(nullptr));
}

void test_partial_update_only_touches_given_fields() {
    SettingsUpdate u;
    u.brightness = "120";
    Settings out;
    const UpdateResult r = applyUpdate(saved(), u, out);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_FALSE(r.wifiChanged);
    TEST_ASSERT_EQUAL_UINT8(120, out.brightness);
    TEST_ASSERT_EQUAL_STRING("Home", out.wifiSsid);
    TEST_ASSERT_EQUAL_STRING("secret123", out.wifiPassword);
}

void test_password_keep_and_change() {
    Settings out;
    SettingsUpdate same;
    same.ssid = "Home";
    same.password = "";
    UpdateResult r = applyUpdate(saved(), same, out);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_FALSE(r.wifiChanged);  // empty password on same network = keep
    TEST_ASSERT_EQUAL_STRING("secret123", out.wifiPassword);

    SettingsUpdate other;
    other.ssid = "Cafe";
    other.password = "";
    r = applyUpdate(saved(), other, out);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_TRUE(r.wifiChanged);
    TEST_ASSERT_EQUAL_STRING("", out.wifiPassword);  // different network: open

    SettingsUpdate newPass;
    newPass.ssid = "Home";
    newPass.password = "another-pass";
    r = applyUpdate(saved(), newPass, out);
    TEST_ASSERT_TRUE(r.wifiChanged);
    TEST_ASSERT_EQUAL_STRING("another-pass", out.wifiPassword);
}

void test_rejections() {
    Settings out;
    auto rejects = [&](SettingsUpdate u) { TEST_ASSERT_FALSE(applyUpdate(saved(), u, out).ok); };
    SettingsUpdate u;
    u.ssid = "";
    rejects(u);
    u = {};
    u.ssid = "123456789012345678901234567890123";  // 33 chars
    rejects(u);
    u = {};
    u.ssid = "Home";
    u.password = "short";
    rejects(u);
    u = {};
    u.starlinkHost = "192.168.100";
    rejects(u);
    u = {};
    u.pollMs = "5000";  // slower than a history slot
    rejects(u);
    u = {};
    u.pollMs = "1000x";
    rejects(u);
    u = {};
    u.brightness = "5";
    rejects(u);
    u = {};
    u.brightness = "256";
    rejects(u);
    u = {};
    u.rotation = "4";
    rejects(u);
    u = {};
    u.rotation = "-1";
    rejects(u);
}

void test_hex_psk_accepted() {
    SettingsUpdate u;
    u.ssid = "Home";
    u.password = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789ABCDEF";
    Settings out;
    TEST_ASSERT_TRUE(applyUpdate(saved(), u, out).ok);
    u.password = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789ABCDEG";
    TEST_ASSERT_FALSE(applyUpdate(saved(), u, out).ok);
}

void test_sanitize() {
    Settings s;
    strcpy(s.starlinkHost, "garbage");
    s.pollMs = 7;
    s.brightness = 0;
    s.rotation = 9;
    sanitize(s);
    TEST_ASSERT_EQUAL_STRING(kDefaultStarlinkHost, s.starlinkHost);
    TEST_ASSERT_EQUAL_UINT16(kDefaultPollMs, s.pollMs);
    TEST_ASSERT_EQUAL_UINT8(kDefaultBrightness, s.brightness);
    TEST_ASSERT_EQUAL_UINT8(kRotationBoardDefault, s.rotation);
}

void test_json_writer() {
    char buf[256];
    JsonWriter j(buf, sizeof(buf));
    j.beginObject()
        .str("ssid", "a\"b\\c\n\x01")
        .num("lat", std::optional<float>(41.6f), 1)
        .num("nan", std::optional<float>(NAN))
        .num("none", std::optional<float>())
        .integer("n", -3)
        .boolean("ok", true)
        .beginArray("list")
        .str(nullptr, "x")
        .integer(nullptr, 2)
        .endArray()
        .beginObject("o")
        .endObject()
        .endObject();
    TEST_ASSERT_TRUE(j.ok());
    TEST_ASSERT_EQUAL_STRING(
        "{\"ssid\":\"a\\\"b\\\\c\\u000a\\u0001\",\"lat\":41.6,\"nan\":null,\"none\":null,\"n\":-3,\"ok\":true,"
        "\"list\":[\"x\",2],\"o\":{}}",
        buf);
}

void test_json_writer_overflow_is_reported() {
    char buf[8];
    JsonWriter j(buf, sizeof(buf));
    j.beginObject().str("key", "long value").endObject();
    TEST_ASSERT_FALSE(j.ok());
    TEST_ASSERT_EQUAL_size_t(7, strlen(buf));  // truncated, still terminated
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ipv4);
    RUN_TEST(test_partial_update_only_touches_given_fields);
    RUN_TEST(test_password_keep_and_change);
    RUN_TEST(test_rejections);
    RUN_TEST(test_hex_psk_accepted);
    RUN_TEST(test_sanitize);
    RUN_TEST(test_json_writer);
    RUN_TEST(test_json_writer_overflow_is_reported);
    return UNITY_END();
}
