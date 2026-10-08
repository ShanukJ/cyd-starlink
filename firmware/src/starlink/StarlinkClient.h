#pragma once

#include <vector>

#include "HistoryDecoder.h"
#include "StarlinkStatus.h"
#include "StarlinkTransport.h"

namespace starlink {

enum class ClientResult : uint8_t {
    Ok,
    TransportError,  // see CallInfo / transportResult()
    ApiError,        // Response.status carried a non-zero code
    NotADish,        // valid response, but no dish_get_status (e.g. a router's IP)
    Malformed,       // protobuf could not be decoded
};

// Synchronous Starlink API client. Knows the request/response messages;
// knows nothing about scheduling, threads or the UI.
class StarlinkClient {
public:
    explicit StarlinkClient(StarlinkTransport& transport) : _transport(transport) {}

    ClientResult getStatus(const char* host, StarlinkStatus& out);

    // Fetches the dish's 15-minute per-second history (~21 KB). `out` points
    // into an internal buffer: use it before the next call, then call
    // releaseBuffers() to give the memory back.
    ClientResult getHistory(const char* host, DishHistory& out);
    void releaseBuffers();

    CallResult transportResult() const { return _lastCall; }
    const CallInfo& callInfo() const { return _info; }
    int32_t apiErrorCode() const { return _apiErrorCode; }

private:
    StarlinkTransport& _transport;
    std::vector<uint8_t> _response;
    CallResult _lastCall = CallResult::Ok;
    CallInfo _info;
    int32_t _apiErrorCode = 0;
};

const char* clientResultName(ClientResult r);

}  // namespace starlink
