#pragma once

#include <stddef.h>
#include <stdint.h>

namespace starlink {

// Minimal, bounds-checked protobuf wire-format reader.
//
// There is no generated code: Starlink publishes no .proto files (the schema
// comes from the dish's gRPC reflection), and a hand-written reader that
// picks out known field numbers and skips everything else is both smaller
// and naturally tolerant of fields being added or removed.
//
//   ProtoReader r(buf, len);
//   while (r.next()) {
//       switch (r.field()) {
//           case 3: apiVersion = r.varint(); break;
//           case 2004: parseDish(r.message()); break;
//       }
//   }
//   if (!r.ok()) { /* malformed */ }
class ProtoReader {
public:
    enum WireType : uint8_t { Varint = 0, Fixed64 = 1, Length = 2, Fixed32 = 5 };

    ProtoReader(const uint8_t* data, size_t len) : _p(data), _end(data + len) {}

    // Advances to the next field. Returns false at the end of the message or
    // on malformed input (then ok() is false).
    bool next();
    bool ok() const { return _ok; }

    uint32_t field() const { return _field; }
    WireType wireType() const { return _wire; }

    // Typed accessors return 0 / empty if the wire type doesn't match, so a
    // field whose type changed in a future API is ignored, not misread.
    uint64_t varint() const { return _wire == Varint ? _varint : 0; }
    bool boolean() const { return varint() != 0; }
    int32_t int32() const { return static_cast<int32_t>(varint()); }
    float float32() const;
    double float64() const;
    const uint8_t* bytes() const { return _wire == Length ? _data : nullptr; }
    size_t length() const { return _wire == Length ? _len : 0; }
    ProtoReader message() const { return _wire == Length ? ProtoReader(_data, _len) : ProtoReader(nullptr, 0); }
    // Copies a string field into `out` (always NUL-terminated, truncated).
    void copyString(char* out, size_t outSize) const;

private:
    bool readVarint(uint64_t& v);

    const uint8_t* _p;
    const uint8_t* _end;
    bool _ok = true;
    uint32_t _field = 0;
    WireType _wire = Varint;
    uint64_t _varint = 0;
    uint32_t _fixed32 = 0;
    uint64_t _fixed64 = 0;
    const uint8_t* _data = nullptr;
    size_t _len = 0;
};

}  // namespace starlink
