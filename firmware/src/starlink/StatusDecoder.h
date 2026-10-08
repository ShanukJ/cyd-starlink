#pragma once

#include <stddef.h>
#include <stdint.h>

#include "StarlinkStatus.h"

namespace starlink {

enum class DecodeResult : uint8_t {
    Ok,
    ApiError,   // Response.status.code != 0
    NotADish,   // a valid Response without dish_get_status
    Malformed,  // not decodable protobuf
};

const char* decodeResultName(DecodeResult r);

// Decodes a SpaceX.API.Device.Response (the protobuf inside the gRPC-Web
// data frame) carrying dish_get_status. Pure C++: no Arduino/ESP-IDF
// dependencies, so it is unit-tested on the host (test/test_status_decoder).
// `out` is reset first; on failure it holds no telemetry.
DecodeResult decodeGetStatus(const uint8_t* data, size_t len, StarlinkStatus& out, int32_t* apiErrorCode = nullptr);

}  // namespace starlink
