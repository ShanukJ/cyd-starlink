#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>

#include <optional>

#include "HistoryDecoder.h"

namespace starlink {

// One time slot of history. NaN = no data for that slot (never 0).
struct HistorySample {
    float downBps = NAN;
    float upBps = NAN;
    float latencyMs = NAN;

    bool empty() const { return isnan(downBps) && isnan(upBps) && isnan(latencyMs); }
};

struct SeriesStats {
    std::optional<float> latest;  // newest valid value
    std::optional<float> mean;
    std::optional<float> max;
};

enum class Series : uint8_t { Down, Up, Latency };

// Fixed-period ring of the last 15 minutes (450 slots of 2 s). Slots are
// tied to time, not to polls: when polls stop, time still advances and the
// skipped slots stay empty, so gaps show as gaps. Pure C++; host-tested.
// Wrap-safe for millis() overflow (unsigned differences only).
class HistoryBuffer {
public:
    static constexpr uint32_t kPeriodMs = 2000;
    static constexpr size_t kSlots = 450;
    static constexpr uint32_t kWindowS = kSlots * kPeriodMs / 1000;

    HistoryBuffer() { clear(); }

    void clear();
    // Moves "now" forward, emptying slots that time has passed over.
    void advanceTo(uint32_t nowMs);
    // Stores a measurement in the current slot (latest wins).
    void record(uint32_t nowMs, const HistorySample& s);
    // Fills *empty* slots from the dish's per-second history (two dish
    // samples per slot, averaged). Locally recorded slots are kept.
    // Returns the number of slots filled.
    size_t backfill(uint32_t nowMs, const DishHistory& h);

    // i = 0 is the oldest slot, kSlots - 1 the current one.
    const HistorySample& at(size_t i) const { return _slots[(_head + 1 + i) % kSlots]; }
    SeriesStats stats(Series s) const;
    uint32_t version() const { return _version; }

    static float value(const HistorySample& s, Series series);

private:
    HistorySample _slots[kSlots];
    size_t _head = 0;  // index of the current slot
    uint32_t _slotStartMs = 0;
    bool _started = false;
    uint32_t _version = 0;
};

}  // namespace starlink
