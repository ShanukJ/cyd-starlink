#pragma once

// Minimal JSON object/array writer into a fixed buffer, with correct string
// escaping. Pure C++ (host-tested in test/test_settings). Output is
// truncated, never overflowed; check ok() before sending.

#include <stddef.h>
#include <stdint.h>

#include <optional>

class JsonWriter {
public:
    JsonWriter(char* buf, size_t size) : _buf(buf), _size(size) {
        if (size) buf[0] = '\0';
    }

    JsonWriter& beginObject(const char* key = nullptr);
    JsonWriter& endObject();
    JsonWriter& beginArray(const char* key = nullptr);
    JsonWriter& endArray();

    JsonWriter& str(const char* key, const char* value);  // null value -> JSON null
    JsonWriter& num(const char* key, double value, int decimals = 0);
    JsonWriter& num(const char* key, const std::optional<float>& value, int decimals = 0);  // nullopt/NaN -> null
    JsonWriter& integer(const char* key, long long value);
    JsonWriter& boolean(const char* key, bool value);
    JsonWriter& null(const char* key);

    const char* c_str() const { return _buf; }
    size_t length() const { return _len; }
    bool ok() const { return !_overflow; }

private:
    void key(const char* k);
    void raw(const char* s);
    void rawChar(char c);
    void escaped(const char* s);

    char* _buf;
    size_t _size;
    size_t _len = 0;
    bool _overflow = false;
    bool _needComma = false;
};
