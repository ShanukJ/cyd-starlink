#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <optional>

#include "StatusDecoder.h"

namespace starlink {

// A packed `repeated float` field, viewed in place (no copy). Valid only
// while the response buffer it points into is alive.
struct FloatArrayView {
    const uint8_t* data = nullptr;
    size_t count = 0;

    float at(size_t i) const {
        float f;
        memcpy(&f, data + 4 * i, sizeof(f));  // unaligned-safe
        return f;
    }
};

// SpaceX.API.Device.DishGetHistoryResponse: per-second ring buffers (900
// samples = 15 min on api 43). `current` counts samples since dish boot;
// the newest sample sits at index (current - 1) % length.
struct DishHistory {
    std::optional<uint64_t> current;
    FloatArrayView popPingLatencyMs;
    FloatArrayView popPingDropRate;
    FloatArrayView downlinkBps;
    FloatArrayView uplinkBps;

    // Number of valid samples in `a` (the dish may have booted < 15 min ago).
    size_t available(const FloatArrayView& a) const;
    // Sample `secondsAgo` (0 = newest) from ring `a`. Call only with
    // secondsAgo < available(a).
    float ago(const FloatArrayView& a, size_t secondsAgo) const;
};

// Decodes a Response carrying dish_get_history. Pure C++; host-tested.
// Arrays whose wire format isn't packed fixed32 are left empty.
DecodeResult decodeGetHistory(const uint8_t* data, size_t len, DishHistory& out, int32_t* apiErrorCode = nullptr);

}  // namespace starlink
