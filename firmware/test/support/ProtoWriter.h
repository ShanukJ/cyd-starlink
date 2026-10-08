#pragma once

// Tiny protobuf encoder for building test messages field by field.

#include <stdint.h>
#include <string.h>

#include <vector>

struct ProtoWriter {
    std::vector<uint8_t> b;

    ProtoWriter& varint(uint64_t v) {
        while (v >= 0x80) {
            b.push_back(static_cast<uint8_t>(v | 0x80));
            v >>= 7;
        }
        b.push_back(static_cast<uint8_t>(v));
        return *this;
    }
    ProtoWriter& tag(uint32_t field, uint8_t wire) { return varint((uint64_t(field) << 3) | wire); }
    ProtoWriter& u(uint32_t field, uint64_t v) { return tag(field, 0).varint(v); }
    ProtoWriter& f32(uint32_t field, float v) {
        tag(field, 5);
        uint8_t t[4];
        memcpy(t, &v, 4);
        b.insert(b.end(), t, t + 4);
        return *this;
    }
    ProtoWriter& bytes(uint32_t field, const uint8_t* data, size_t len) {
        tag(field, 2).varint(len);
        b.insert(b.end(), data, data + len);
        return *this;
    }
    ProtoWriter& str(uint32_t field, const char* s) {
        return bytes(field, reinterpret_cast<const uint8_t*>(s), strlen(s));
    }
    ProtoWriter& msg(uint32_t field, const ProtoWriter& m) { return bytes(field, m.b.data(), m.b.size()); }
};

// SpaceX.API.Device.Response { api_version = 3; dish_get_status = 2004 }
inline ProtoWriter dishResponse(const ProtoWriter& dish) {
    ProtoWriter r;
    r.u(3, 43).msg(2004, dish);
    return r;
}
