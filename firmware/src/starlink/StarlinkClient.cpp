#include "StarlinkClient.h"

#include "StatusDecoder.h"

namespace starlink {

namespace {

// SpaceX.API.Device.Request { GetStatusRequest get_status = 1004; }
// Tag (1004 << 3 | 2) = 8034 = varint E2 3E, then a zero-length message.
constexpr uint8_t kGetStatusRequest[] = {0xE2, 0x3E, 0x00};

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
    _lastCall = _transport.call(host, kGetStatusRequest, sizeof(kGetStatusRequest), _response, _info);
    if (_lastCall != CallResult::Ok) return ClientResult::TransportError;

    switch (decodeGetStatus(_response.data(), _response.size(), out, &_apiErrorCode)) {
        case DecodeResult::Ok: return ClientResult::Ok;
        case DecodeResult::ApiError: return ClientResult::ApiError;
        case DecodeResult::NotADish: return ClientResult::NotADish;
        case DecodeResult::Malformed: break;
    }
    return ClientResult::Malformed;
}

}  // namespace starlink
