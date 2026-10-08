#include "StarlinkClient.h"

#include "StatusDecoder.h"

namespace starlink {

namespace {

// SpaceX.API.Device.Request { GetStatusRequest get_status = 1004; }
// Tag (1004 << 3 | 2) = 8034 = varint E2 3E, then a zero-length message.
constexpr uint8_t kGetStatusRequest[] = {0xE2, 0x3E, 0x00};

// SpaceX.API.Device.Request { GetHistoryRequest get_history = 1007; }
// Tag (1007 << 3 | 2) = 8058 = varint FA 3E, then a zero-length message.
constexpr uint8_t kGetHistoryRequest[] = {0xFA, 0x3E, 0x00};

// Expected HTTP body sizes (api 43): status ~0.6 KB, history ~21.2 KB.
constexpr size_t kStatusSizeHint = 1024;
constexpr size_t kHistorySizeHint = 22 * 1024;

ClientResult toClientResult(DecodeResult r) {
    switch (r) {
        case DecodeResult::Ok: return ClientResult::Ok;
        case DecodeResult::ApiError: return ClientResult::ApiError;
        case DecodeResult::NotADish: return ClientResult::NotADish;
        case DecodeResult::Malformed: break;
    }
    return ClientResult::Malformed;
}

}  // namespace

const char* clientResultName(ClientResult r) {
    switch (r) {
        case ClientResult::Ok: return "OK";
        case ClientResult::TransportError: return "TRANSPORT_ERROR";
        case ClientResult::ApiError: return "API_ERROR";
        case ClientResult::NotADish: return "NOT_A_DISH";
        case ClientResult::Malformed: return "MALFORMED";
    }
    return "UNKNOWN";
}

ClientResult StarlinkClient::getStatus(const char* host, StarlinkStatus& out) {
    out = StarlinkStatus{};
    _apiErrorCode = 0;
    _lastCall = _transport.call(host, kGetStatusRequest, sizeof(kGetStatusRequest), kStatusSizeHint, _response, _info);
    if (_lastCall != CallResult::Ok) return ClientResult::TransportError;

    return toClientResult(decodeGetStatus(_response.data(), _response.size(), out, &_apiErrorCode));
}

ClientResult StarlinkClient::getHistory(const char* host, DishHistory& out) {
    out = DishHistory{};
    _apiErrorCode = 0;
    _lastCall = _transport.call(host, kGetHistoryRequest, sizeof(kGetHistoryRequest), kHistorySizeHint, _response,
                               _info);
    if (_lastCall != CallResult::Ok) return ClientResult::TransportError;
    return toClientResult(decodeGetHistory(_response.data(), _response.size(), out, &_apiErrorCode));
}

void StarlinkClient::releaseBuffers() {
    std::vector<uint8_t>().swap(_response);
    _transport.releaseBuffers();
}

}  // namespace starlink
