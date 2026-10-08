#pragma once

#include "StarlinkTransport.h"

namespace starlink {

// gRPC-Web over plain HTTP/1.1 to the dish's port 9201 — the same endpoint
// the Starlink web app uses. Chosen over native gRPC (port 9200) because
// native gRPC needs HTTP/2; gRPC-Web is a simple POST. See
// docs/protocol/starlink-grpc-web.md for the observed wire behaviour.
class GrpcWebTransport : public StarlinkTransport {
public:
    static constexpr uint16_t kDefaultPort = 9201;

    struct Options {
        uint16_t port = kDefaultPort;
        uint32_t connectTimeoutMs = 2000;
        uint32_t responseTimeoutMs = 4000;
        size_t maxResponseBytes = 16 * 1024;
    };

    GrpcWebTransport() = default;
    explicit GrpcWebTransport(const Options& o) : _opt(o) {}

    CallResult call(const char* host, const uint8_t* request, size_t requestLen, std::vector<uint8_t>& response,
                    CallInfo& info) override;

private:
    Options _opt;
    std::vector<uint8_t> _body;  // reused HTTP body buffer
};

}  // namespace starlink
