#include "JsonWriter.h"

#include <math.h>
#include <stdio.h>

void JsonWriter::rawChar(char c) {
    if (_len + 1 >= _size) {
        _overflow = true;
        return;
    }
    _buf[_len++] = c;
    _buf[_len] = '\0';
}

void JsonWriter::raw(const char* s) {
    while (*s) rawChar(*s++);
}

void JsonWriter::escaped(const char* s) {
    rawChar('"');
    for (; *s; ++s) {
        const unsigned char c = static_cast<unsigned char>(*s);
        if (c == '"' || c == '\\') {
            rawChar('\\');
            rawChar(static_cast<char>(c));
        } else if (c < 0x20) {
            char u[8];
            snprintf(u, sizeof(u), "\\u%04x", c);
            raw(u);
        } else {
            rawChar(static_cast<char>(c));  // UTF-8 passes through unchanged
        }
    }
    rawChar('"');
}

void JsonWriter::key(const char* k) {
    if (_needComma) rawChar(',');
    _needComma = true;
    if (k) {
        escaped(k);
        rawChar(':');
    }
}

JsonWriter& JsonWriter::beginObject(const char* k) {
    key(k);
    rawChar('{');
    _needComma = false;
    return *this;
}

JsonWriter& JsonWriter::endObject() {
    rawChar('}');
    _needComma = true;
    return *this;
}

JsonWriter& JsonWriter::beginArray(const char* k) {
    key(k);
    rawChar('[');
    _needComma = false;
    return *this;
}

JsonWriter& JsonWriter::endArray() {
    rawChar(']');
    _needComma = true;
    return *this;
}

JsonWriter& JsonWriter::str(const char* k, const char* value) {
    if (!value) return null(k);
    key(k);
    escaped(value);
    return *this;
}

JsonWriter& JsonWriter::num(const char* k, double value, int decimals) {
    if (!isfinite(value)) return null(k);  // JSON has no NaN/Inf
    key(k);
    char b[32];
    snprintf(b, sizeof(b), "%.*f", decimals, value);
    raw(b);
    return *this;
}

JsonWriter& JsonWriter::num(const char* k, const std::optional<float>& value, int decimals) {
    if (!value) return null(k);
    return num(k, static_cast<double>(*value), decimals);
}

JsonWriter& JsonWriter::integer(const char* k, long long value) {
    key(k);
    char b[24];
    snprintf(b, sizeof(b), "%lld", value);
    raw(b);
    return *this;
}

JsonWriter& JsonWriter::boolean(const char* k, bool value) {
    key(k);
    raw(value ? "true" : "false");
    return *this;
}

JsonWriter& JsonWriter::null(const char* k) {
    key(k);
    raw("null");
    return *this;
}
