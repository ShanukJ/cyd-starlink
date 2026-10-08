#include "ProtoReader.h"

#include <string.h>

namespace starlink {

bool ProtoReader::readVarint(uint64_t& v) {
    v = 0;
    for (int shift = 0; shift < 64; shift += 7) {
        if (_p >= _end) return false;
        const uint8_t b = *_p++;
        v |= static_cast<uint64_t>(b & 0x7F) << shift;
        if (!(b & 0x80)) return true;
    }
    return false;  // more than 10 bytes
}

bool ProtoReader::next() {
    if (!_ok || _p == nullptr || _p >= _end) return false;

    uint64_t tag;
    if (!readVarint(tag) || (tag >> 3) == 0 || (tag >> 3) > 0x1FFFFFFF) {
        _ok = false;
        return false;
    }
    _field = static_cast<uint32_t>(tag >> 3);
    const uint8_t wire = tag & 7;

    switch (wire) {
        case Varint:
            _wire = Varint;
            if (!readVarint(_varint)) break;
            return true;
        case Fixed64:
            _wire = Fixed64;
            if (_end - _p < 8) break;
            memcpy(&_fixed64, _p, 8);  // little-endian on the wire and on ESP32
            _p += 8;
            return true;
        case Length: {
            _wire = Length;
            uint64_t len;
            if (!readVarint(len) || len > static_cast<uint64_t>(_end - _p)) break;
            _data = _p;
            _len = static_cast<size_t>(len);
            _p += _len;
            return true;
        }
        case Fixed32:
            _wire = Fixed32;
            if (_end - _p < 4) break;
            memcpy(&_fixed32, _p, 4);
            _p += 4;
            return true;
        default:
            break;  // groups (3/4) are deprecated and never used by Starlink
    }
    _ok = false;
    return false;
}

float ProtoReader::float32() const {
    if (_wire != Fixed32) return 0.0f;
    float f;
    memcpy(&f, &_fixed32, sizeof(f));
    return f;
}

double ProtoReader::float64() const {
    if (_wire != Fixed64) return 0.0;
    double d;
    memcpy(&d, &_fixed64, sizeof(d));
    return d;
}

void ProtoReader::copyString(char* out, size_t outSize) const {
    if (outSize == 0) return;
    size_t n = length();
    if (n >= outSize) n = outSize - 1;
    if (n) memcpy(out, _data, n);
    out[n] = '\0';
}

}  // namespace starlink
