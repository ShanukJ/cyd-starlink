#include "GrpcWebTransport.h"

#include <WiFi.h>
#include <strings.h>

namespace starlink {

const char* callResultName(CallResult r) {
    switch (r) {
        case CallResult::Ok: return "OK";
        case CallResult::ConnectFailed: return "CONNECT_FAILED";
        case CallResult::Timeout: return "TIMEOUT";
        case CallResult::HttpError: return "HTTP_ERROR";
        case CallResult::GrpcError: return "GRPC_ERROR";
        case CallResult::Malformed: return "MALFORMED";
        case CallResult::TooLarge: return "TOO_LARGE";
    }
    return "UNKNOWN";
}

namespace {

constexpr const char* kPath = "/SpaceX.API.Device.Device/Handle";
constexpr uint8_t kFrameTrailer = 0x80;
constexpr uint8_t kFrameCompressed = 0x01;

// Deadline-bounded reads from a TCP client. Never blocks past the deadline.
class Reader {
public:
    Reader(WiFiClient& c, uint32_t deadline) : _c(c), _deadline(deadline) {}

    bool timedOut() const { return _timedOut; }

    // Waits until data is available. False on timeout or peer close.
    bool waitData() {
        for (;;) {
            if (_c.available() > 0) return true;
            if (!_c.connected()) return false;
            if (static_cast<int32_t>(millis() - _deadline) >= 0) {
                _timedOut = true;
                return false;
            }
            vTaskDelay(1);
        }
    }

    // Reads one CRLF/LF-terminated line without the terminator. Overlong
    // lines are truncated (the rest is discarded).
    bool readLine(char* buf, size_t size) {
        size_t n = 0;
        for (;;) {
            if (!waitData()) return false;
            const int ch = _c.read();
            if (ch < 0) continue;
            if (ch == '\n') break;
            if (ch != '\r' && n + 1 < size) buf[n++] = static_cast<char>(ch);
        }
        buf[n] = '\0';
        return true;
    }

    bool readExact(uint8_t* dst, size_t len) {
        while (len) {
            if (!waitData()) return false;
            const int got = _c.read(dst, len);
            if (got <= 0) continue;
            dst += got;
            len -= got;
        }
        return true;
    }

private:
    WiFiClient& _c;
    uint32_t _deadline;
    bool _timedOut = false;
};

// "Name: value" header match, case-insensitive. Returns the value or null.
const char* headerValue(const char* line, const char* name) {
    const size_t n = strlen(name);
    if (strncasecmp(line, name, n) != 0 || line[n] != ':') return nullptr;
    const char* v = line + n + 1;
    while (*v == ' ' || *v == '\t') ++v;
    return v;
}

// Parses "grpc-status" / "grpc-message" from an HTTP header line or a
// gRPC-Web trailer line (same "name: value" format).
void parseGrpcHeader(const char* line, CallInfo& info) {
    if (const char* v = headerValue(line, "grpc-status")) {
        info.grpcStatus = atoi(v);
    } else if (const char* v = headerValue(line, "grpc-message")) {
        strlcpy(info.grpcMessage, v, sizeof(info.grpcMessage));
    }
}

}  // namespace

CallResult GrpcWebTransport::call(const char* host, const uint8_t* request, size_t requestLen,
                                  std::vector<uint8_t>& response, CallInfo& info) {
    const uint32_t start = millis();
    info = CallInfo{};
    response.clear();
    WiFiClient client;
    auto finish = [&](CallResult r) {
        client.stop();
        info.elapsedMs = millis() - start;
        return r;
    };

    IPAddress ip;
    if (!ip.fromString(host)) return finish(CallResult::ConnectFailed);
    if (!client.connect(ip, _opt.port, _opt.connectTimeoutMs)) return finish(CallResult::ConnectFailed);
    client.setNoDelay(true);

    // Request: HTTP headers, then one gRPC-Web data frame
    // (1 flag byte + 4-byte big-endian length + protobuf).
    char head[224];
    const int headLen = snprintf(head, sizeof(head),
                                 "POST %s HTTP/1.1\r\n"
                                 "Host: %s:%u\r\n"
                                 "Content-Type: application/grpc-web+proto\r\n"
                                 "X-Grpc-Web: 1\r\n"
                                 "Content-Length: %u\r\n"
                                 "Connection: close\r\n\r\n",
                                 kPath, host, _opt.port, static_cast<unsigned>(5 + requestLen));
    const uint8_t frame[5] = {0, static_cast<uint8_t>(requestLen >> 24), static_cast<uint8_t>(requestLen >> 16),
                              static_cast<uint8_t>(requestLen >> 8), static_cast<uint8_t>(requestLen)};
    if (client.write(reinterpret_cast<const uint8_t*>(head), headLen) != static_cast<size_t>(headLen) ||
        client.write(frame, sizeof(frame)) != sizeof(frame) ||
        client.write(request, requestLen) != requestLen) {
        return finish(CallResult::ConnectFailed);
    }

    Reader rd(client, start + _opt.responseTimeoutMs);
    auto readFailure = [&] { return finish(rd.timedOut() ? CallResult::Timeout : CallResult::HttpError); };

    // Status line
    char line[160];
    if (!rd.readLine(line, sizeof(line))) return readFailure();
    int httpCode = 0;
    if (sscanf(line, "HTTP/%*d.%*d %d", &httpCode) != 1) return finish(CallResult::HttpError);
    if (httpCode != 200) return finish(CallResult::HttpError);

    // Headers. Errors arrive as a "trailers-only" response: grpc-status in
    // the HTTP headers and an empty body (observed on firmware 2026.09).
    bool chunked = false;
    long contentLength = -1;
    for (;;) {
        if (!rd.readLine(line, sizeof(line))) return readFailure();
        if (line[0] == '\0') break;
        if (const char* v = headerValue(line, "transfer-encoding")) {
            chunked = strcasestr(v, "chunked") != nullptr;
        } else if (const char* v = headerValue(line, "content-length")) {
            contentLength = atol(v);
        } else {
            parseGrpcHeader(line, info);
        }
    }

    // Body (the dish always uses chunked encoding, but don't depend on it).
    _body.clear();
    auto readBytes = [&](size_t n) -> int {  // 1 ok, 0 too large, -1 read failure
        if (_body.size() + n > _opt.maxResponseBytes) return 0;
        const size_t at = _body.size();
        _body.resize(at + n);
        return rd.readExact(_body.data() + at, n) ? 1 : -1;
    };
    if (chunked) {
        for (;;) {
            if (!rd.readLine(line, sizeof(line))) return readFailure();
            char* endp = nullptr;
            const unsigned long size = strtoul(line, &endp, 16);
            if (endp == line) return finish(CallResult::Malformed);
            if (size == 0) {
                while (rd.readLine(line, sizeof(line)) && line[0] != '\0') {}  // HTTP trailers
                break;
            }
            const int r = readBytes(size);
            if (r == 0) return finish(CallResult::TooLarge);
            if (r < 0) return readFailure();
            if (!rd.readLine(line, sizeof(line))) return readFailure();  // chunk CRLF
        }
    } else if (contentLength >= 0) {
        const int r = readBytes(static_cast<size_t>(contentLength));
        if (r == 0) return finish(CallResult::TooLarge);
        if (r < 0) return readFailure();
    } else {
        uint8_t buf[256];
        while (rd.waitData()) {
            const int got = client.read(buf, sizeof(buf));
            if (got <= 0) continue;
            if (_body.size() + got > _opt.maxResponseBytes) return finish(CallResult::TooLarge);
            _body.insert(_body.end(), buf, buf + got);
        }
        if (rd.timedOut()) return finish(CallResult::Timeout);
    }

    // gRPC-Web frames: data frame(s) with the protobuf, then a trailer frame
    // carrying "grpc-status: N".
    size_t off = 0;
    while (off + 5 <= _body.size()) {
        const uint8_t flags = _body[off];
        const uint32_t len = (uint32_t(_body[off + 1]) << 24) | (uint32_t(_body[off + 2]) << 16) |
                             (uint32_t(_body[off + 3]) << 8) | _body[off + 4];
        off += 5;
        if (len > _body.size() - off || (flags & kFrameCompressed)) return finish(CallResult::Malformed);
        if (flags & kFrameTrailer) {
            // Trailer block: CRLF-separated "name: value" lines.
            const char* p = reinterpret_cast<const char*>(_body.data() + off);
            const char* end = p + len;
            while (p < end) {
                const char* eol = static_cast<const char*>(memchr(p, '\n', end - p));
                const size_t n = (eol ? eol : end) - p;
                const size_t copy = n < sizeof(line) - 1 ? n : sizeof(line) - 1;
                memcpy(line, p, copy);
                line[copy] = '\0';
                if (copy && line[copy - 1] == '\r') line[copy - 1] = '\0';
                parseGrpcHeader(line, info);
                p += n + 1;
            }
        } else {
            response.insert(response.end(), _body.begin() + off, _body.begin() + off + len);
        }
        off += len;
    }
    if (off != _body.size()) return finish(CallResult::Malformed);

    if (info.grpcStatus > 0) return finish(CallResult::GrpcError);
    if (response.empty()) return finish(CallResult::Malformed);
    return finish(CallResult::Ok);
}

}  // namespace starlink
