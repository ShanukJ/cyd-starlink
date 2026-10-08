#include "HistoryBuffer.h"

namespace starlink {

namespace {

// Mean of the valid (non-NaN, finite) values; NaN if none.
float meanOf(float a, float b) {
    const bool va = isfinite(a), vb = isfinite(b);
    if (va && vb) return (a + b) / 2;
    if (va) return a;
    if (vb) return b;
    return NAN;
}

float sampleAt(const DishHistory& h, const FloatArrayView& a, size_t secondsAgo) {
    if (secondsAgo >= h.available(a)) return NAN;
    const float v = h.ago(a, secondsAgo);
    return isfinite(v) ? v : NAN;
}

}  // namespace

void HistoryBuffer::clear() {
    for (HistorySample& s : _slots) s = HistorySample{};
    _head = 0;
    _started = false;
    _version++;
}

void HistoryBuffer::advanceTo(uint32_t nowMs) {
    if (!_started) {
        _started = true;
        _slotStartMs = nowMs;
        return;
    }
    const uint32_t steps = (nowMs - _slotStartMs) / kPeriodMs;
    if (steps == 0) return;
    const uint32_t clearCount = steps < kSlots ? steps : kSlots;
    for (uint32_t i = 0; i < clearCount; ++i) {
        _head = (_head + 1) % kSlots;
        _slots[_head] = HistorySample{};
    }
    _slotStartMs += steps * kPeriodMs;  // keep the slot phase exact
    _version++;
}

void HistoryBuffer::record(uint32_t nowMs, const HistorySample& s) {
    advanceTo(nowMs);
    _slots[_head] = s;
    _version++;
}

size_t HistoryBuffer::backfill(uint32_t nowMs, const DishHistory& h) {
    advanceTo(nowMs);
    size_t filled = 0;
    for (size_t slotsAgo = 0; slotsAgo < kSlots; ++slotsAgo) {
        const size_t s0 = slotsAgo * 2, s1 = s0 + 1;  // two 1 s dish samples per 2 s slot
        HistorySample d;
        d.downBps = meanOf(sampleAt(h, h.downlinkBps, s0), sampleAt(h, h.downlinkBps, s1));
        d.upBps = meanOf(sampleAt(h, h.uplinkBps, s0), sampleAt(h, h.uplinkBps, s1));
        d.latencyMs = meanOf(sampleAt(h, h.popPingLatencyMs, s0), sampleAt(h, h.popPingLatencyMs, s1));
        // The dish reports latency 0 when no ping succeeded: not a reading.
        if (d.latencyMs == 0.0f) d.latencyMs = NAN;
        if (d.empty()) continue;
        HistorySample& slot = _slots[(_head + kSlots - slotsAgo) % kSlots];
        if (!slot.empty()) continue;  // keep what we measured ourselves
        slot = d;
        filled++;
    }
    if (filled) _version++;
    return filled;
}

float HistoryBuffer::value(const HistorySample& s, Series series) {
    switch (series) {
        case Series::Down: return s.downBps;
        case Series::Up: return s.upBps;
        case Series::Latency: return s.latencyMs;
    }
    return NAN;
}

SeriesStats HistoryBuffer::stats(Series series) const {
    SeriesStats st;
    double sum = 0;
    size_t n = 0;
    float mx = -INFINITY;
    for (size_t i = 0; i < kSlots; ++i) {
        const float v = value(at(i), series);
        if (!isfinite(v)) continue;
        sum += v;
        n++;
        if (v > mx) mx = v;
        st.latest = v;  // ascending order: the last valid one is the newest
    }
    if (n) {
        st.mean = static_cast<float>(sum / n);
        st.max = mx;
    }
    return st;
}

}  // namespace starlink
