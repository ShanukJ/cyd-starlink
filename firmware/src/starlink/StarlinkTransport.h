#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace starlink {

enum class CallResult : uint8_t {
    Ok,
    ConnectFailed,  // TCP connect refused / unreachable / timed out
    Timeout,        // connected, but the response did not complete in time
    HttpError,      // non-200 status or unparsable HTTP
    GrpcError,      // the dish answered with a non-zero grpc-status
    Malformed,      // body framing was invalid
    TooLarge,       // response exceeded the buffer limit
};

const char* callResultName(CallResult r);

struct CallInfo {
    int grpcStatus = -1;      // 0 = OK; -1 = not received
    char grpcMessage[64] = "";
    uint32_t elapsedMs = 0;
};

// Sends one unary SpaceX.API.Device.Device/Handle call and returns the
// response protobuf (a SpaceX.API.Device.Response). Implementations are
// blocking with bounded timeouts and are only used from the network task.
class StarlinkTransport {
public:
    virtual ~StarlinkTransport() = default;
    virtual CallResult call(const char* host, const uint8_t* request, size_t requestLen,
                            std::vector<uint8_t>& response, CallInfo& info) = 0;
};

}  // namespace starlink
