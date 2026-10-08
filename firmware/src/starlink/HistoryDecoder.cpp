#include "HistoryDecoder.h"

#include "ProtoReader.h"

// Field numbers: dish gRPC reflection, api_version 43 (2026-10).
//   Response { status = 2; api_version = 3; dish_get_history = 2006; }
//   DishGetHistoryResponse { uint64 current = 1;
//     repeated float pop_ping_drop_rate = 1001; pop_ping_latency_ms = 1002;
//     downlink_throughput_bps = 1003; uplink_throughput_bps = 1004; ... }

namespace starlink {

namespace {

namespace F {
constexpr uint32_t kRespStatus = 2, kRespDishGetHistory = 2006;
constexpr uint32_t kStatusCode = 1;
constexpr uint32_t kCurrent = 1, kDropRate = 1001, kLatency = 1002, kDownlink = 1003, kUplink = 1004;
}  // namespace F

void takeFloats(FloatArrayView& v, const ProtoReader& r) {
    // Packed encoding only (what the dish sends); anything else => empty.
    if (r.wireType() != ProtoReader::Length || r.length() % 4 != 0) return;
    v.data = r.bytes();
    v.count = r.length() / 4;
}

bool decodeHistory(ProtoReader r, DishHistory& out) {
    while (r.next()) {
        switch (r.field()) {
            case F::kCurrent:
                if (r.wireType() == ProtoReader::Varint) out.current = r.varint();
                break;
            case F::kDropRate: takeFloats(out.popPingDropRate, r); break;
            case F::kLatency: takeFloats(out.popPingLatencyMs, r); break;
            case F::kDownlink: takeFloats(out.downlinkBps, r); break;
            case F::kUplink: takeFloats(out.uplinkBps, r); break;
            default: break;
        }
    }
    return r.ok();
}

}  // namespace

size_t DishHistory::available(const FloatArrayView& a) const {
    if (!current || a.count == 0) return 0;
    return *current < a.count ? static_cast<size_t>(*current) : a.count;
}

float DishHistory::ago(const FloatArrayView& a, size_t secondsAgo) const {
    const uint64_t newest = *current - 1;
    return a.at(static_cast<size_t>((newest - secondsAgo) % a.count));
}

DecodeResult decodeGetHistory(const uint8_t* data, size_t len, DishHistory& out, int32_t* apiErrorCode) {
    out = DishHistory{};
    int32_t apiError = 0;
    bool haveHistory = false;
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
            case F::kRespDishGetHistory:
                if (r.wireType() != ProtoReader::Length) {
                    ok = false;
                    break;
                }
                haveHistory = true;
                ok = decodeHistory(r.message(), out);
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
    } else if (!haveHistory) {
        result = DecodeResult::NotADish;
    }
    if (result != DecodeResult::Ok) out = DishHistory{};
    return result;
}

}  // namespace starlink
