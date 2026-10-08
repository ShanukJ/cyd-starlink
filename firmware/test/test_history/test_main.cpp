// Host tests for the dish history decoder and the local HistoryBuffer.

#include <math.h>
#include <unity.h>

#include "../support/ProtoWriter.h"
#include "starlink/HistoryBuffer.h"
#include "starlink/HistoryDecoder.h"

using namespace starlink;

void setUp() {}
void tearDown() {}

// Packed repeated float field.
static void packed(ProtoWriter& w, uint32_t field, const std::vector<float>& v) {
    w.bytes(field, reinterpret_cast<const uint8_t*>(v.data()), v.size() * 4);
}

// A history response with ring length `n`, `current` samples taken, where
// the sample taken at absolute time t (seconds since dish boot) has value
// t for downlink, 10*t for uplink and 100+t for latency.
static ProtoWriter historyResponse(size_t n, uint64_t current) {
    std::vector<float> down(n, 0), up(n, 0), lat(n, 0);
    for (uint64_t t = 0; t < current; ++t) {
        down[t % n] = static_cast<float>(t);
        up[t % n] = static_cast<float>(10 * t);
        lat[t % n] = static_cast<float>(100 + t);
    }
    ProtoWriter h;
    h.u(1, current);
    packed(h, 1002, lat);
    packed(h, 1003, down);
    packed(h, 1004, up);
    ProtoWriter r;
    r.u(3, 43).msg(2006, h);
    return r;
}

void test_decode_ring_order_wrapped() {
    const ProtoWriter r = historyResponse(900, 109918);  // wrapped many times
    DishHistory h;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecodeResult::Ok),
                          static_cast<int>(decodeGetHistory(r.b.data(), r.b.size(), h)));
    TEST_ASSERT_EQUAL_UINT64(109918, *h.current);
    TEST_ASSERT_EQUAL_size_t(900, h.available(h.downlinkBps));
    TEST_ASSERT_EQUAL_FLOAT(109917.0f, h.ago(h.downlinkBps, 0));  // newest
    TEST_ASSERT_EQUAL_FLOAT(109917.0f - 899.0f, h.ago(h.downlinkBps, 899));  // oldest
    TEST_ASSERT_EQUAL_FLOAT(100.0f + 109917.0f, h.ago(h.popPingLatencyMs, 0));
}

void test_decode_dish_booted_recently() {
    const ProtoWriter r = historyResponse(900, 30);  // only 30 s of data
    DishHistory h;
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(decodeGetHistory(r.b.data(), r.b.size(), h)));
    TEST_ASSERT_EQUAL_size_t(30, h.available(h.uplinkBps));
    TEST_ASSERT_EQUAL_FLOAT(290.0f, h.ago(h.uplinkBps, 0));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, h.ago(h.uplinkBps, 29));
}

void test_decode_bad_input() {
    DishHistory h;
    ProtoWriter notHistory;
    notHistory.u(3, 43).msg(2004, ProtoWriter{});  // a status response
    TEST_ASSERT_EQUAL_INT(static_cast<int>(DecodeResult::NotADish),
                          static_cast<int>(decodeGetHistory(notHistory.b.data(), notHistory.b.size(), h)));
    ProtoWriter odd;  // packed float field with a length that isn't a multiple of 4
    ProtoWriter hist;
    hist.u(1, 5);
    const uint8_t three[] = {1, 2, 3};
    hist.bytes(1003, three, 3);
    odd.u(3, 43).msg(2006, hist);
    TEST_ASSERT_EQUAL_INT(0, static_cast<int>(decodeGetHistory(odd.b.data(), odd.b.size(), h)));
    TEST_ASSERT_EQUAL_size_t(0, h.available(h.downlinkBps));

    const ProtoWriter full = historyResponse(900, 5000);
    for (size_t len = 0; len < full.b.size(); len += 97) {  // truncations must fail cleanly
        std::vector<uint8_t> cut(full.b.begin(), full.b.begin() + len);
        decodeGetHistory(cut.data(), cut.size(), h);
    }
}

void test_buffer_records_and_gaps() {
    HistoryBuffer b;
    uint32_t now = 1000;
    b.record(now, {1e6f, 2e5f, 40.0f});
    TEST_ASSERT_EQUAL_FLOAT(1e6f, b.at(HistoryBuffer::kSlots - 1).downBps);

    // 10 s with no data: 5 empty slots, then a new sample.
    now += 10000;
    b.record(now, {3e6f, 1e5f, 50.0f});
    TEST_ASSERT_EQUAL_FLOAT(3e6f, b.at(HistoryBuffer::kSlots - 1).downBps);
    for (size_t i = 2; i <= 5; ++i) TEST_ASSERT_TRUE(b.at(HistoryBuffer::kSlots - i).empty());
    TEST_ASSERT_EQUAL_FLOAT(1e6f, b.at(HistoryBuffer::kSlots - 6).downBps);

    const SeriesStats st = b.stats(Series::Down);
    TEST_ASSERT_EQUAL_FLOAT(3e6f, *st.latest);
    TEST_ASSERT_EQUAL_FLOAT(3e6f, *st.max);
    TEST_ASSERT_EQUAL_FLOAT(2e6f, *st.mean);
}

void test_buffer_long_outage_clears_everything() {
    HistoryBuffer b;
    b.record(0, {1e6f, 1e6f, 30.0f});
    b.advanceTo(HistoryBuffer::kWindowS * 1000 + 5000);
    TEST_ASSERT_FALSE(b.stats(Series::Down).latest.has_value());
}

void test_buffer_survives_millis_wrap() {
    HistoryBuffer b;
    const uint32_t nearWrap = 0xFFFFFFFFu - 3000;
    b.record(nearWrap, {1.0f, 1.0f, 1.0f});
    b.record(nearWrap + 4000, {2.0f, 2.0f, 2.0f});  // wraps past 0
    TEST_ASSERT_EQUAL_FLOAT(2.0f, b.at(HistoryBuffer::kSlots - 1).downBps);
    TEST_ASSERT_TRUE(b.at(HistoryBuffer::kSlots - 2).empty());
    TEST_ASSERT_EQUAL_FLOAT(1.0f, b.at(HistoryBuffer::kSlots - 3).downBps);
}

void test_backfill_fills_only_empty_slots() {
    HistoryBuffer b;
    const uint32_t now = 100000;
    b.record(now, {123.0f, 456.0f, 7.0f});  // our own measurement in the current slot

    const ProtoWriter r = historyResponse(900, 109918);
    DishHistory h;
    decodeGetHistory(r.b.data(), r.b.size(), h);
    const size_t filled = b.backfill(now, h);
    TEST_ASSERT_EQUAL_size_t(HistoryBuffer::kSlots - 1, filled);

    // Current slot keeps the local sample.
    TEST_ASSERT_EQUAL_FLOAT(123.0f, b.at(HistoryBuffer::kSlots - 1).downBps);
    // Previous slot = dish seconds-ago 2 and 3 averaged.
    TEST_ASSERT_EQUAL_FLOAT((109915.0f + 109914.0f) / 2, b.at(HistoryBuffer::kSlots - 2).downBps);
    TEST_ASSERT_EQUAL_FLOAT(100.0f + (109915.0f + 109914.0f) / 2, b.at(HistoryBuffer::kSlots - 2).latencyMs);
    // Oldest slot = dish seconds-ago 898 and 899.
    TEST_ASSERT_EQUAL_FLOAT((109917.0f - 898 + 109917.0f - 899) / 2, b.at(0).downBps);
}

void test_backfill_short_history_and_zero_latency() {
    HistoryBuffer b;
    ProtoWriter hist;
    hist.u(1, 4);
    packed(hist, 1002, {0.0f, 0.0f, 30.0f, 32.0f, 0, 0});  // first two pings failed
    packed(hist, 1003, {5, 6, 7, 8, 0, 0});
    ProtoWriter r;
    r.u(3, 43).msg(2006, hist);
    DishHistory h;
    decodeGetHistory(r.b.data(), r.b.size(), h);
    TEST_ASSERT_EQUAL_size_t(2, b.backfill(50000, h));
    TEST_ASSERT_EQUAL_FLOAT(7.5f, b.at(HistoryBuffer::kSlots - 1).downBps);
    TEST_ASSERT_EQUAL_FLOAT(31.0f, b.at(HistoryBuffer::kSlots - 1).latencyMs);
    TEST_ASSERT_EQUAL_FLOAT(5.5f, b.at(HistoryBuffer::kSlots - 2).downBps);
    TEST_ASSERT_TRUE(isnan(b.at(HistoryBuffer::kSlots - 2).latencyMs));  // 0 = no ping, not 0 ms
    TEST_ASSERT_TRUE(isnan(b.at(HistoryBuffer::kSlots - 2).upBps));  // uplink array absent
    TEST_ASSERT_TRUE(b.at(HistoryBuffer::kSlots - 3).empty());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_decode_ring_order_wrapped);
    RUN_TEST(test_decode_dish_booted_recently);
    RUN_TEST(test_decode_bad_input);
    RUN_TEST(test_buffer_records_and_gaps);
    RUN_TEST(test_buffer_long_outage_clears_everything);
    RUN_TEST(test_buffer_survives_millis_wrap);
    RUN_TEST(test_backfill_fills_only_empty_slots);
    RUN_TEST(test_backfill_short_history_and_zero_latency);
    return UNITY_END();
}
