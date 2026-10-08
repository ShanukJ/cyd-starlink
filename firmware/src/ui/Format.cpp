#include "Format.h"

#include <stdio.h>
#include <string.h>

namespace ui::fmt {

namespace {
void unavailable(char* out, size_t size) { snprintf(out, size, "%s", kUnavailable); }
}  // namespace

void throughput(const std::optional<float>& bps, char* out, size_t size, const char** unit) {
    *unit = "Mbps";
    if (!bps) return unavailable(out, size);
    const float v = *bps;
    if (v < 1e6f) {
        *unit = "kbps";
        snprintf(out, size, "%.0f", v / 1e3f);
    } else if (v < 9.95e6f) {  // rounds to 10.0 and above => integer format
        snprintf(out, size, "%.1f", v / 1e6f);
    } else {
        snprintf(out, size, "%.0f", v / 1e6f);
    }
}

void latency(const std::optional<float>& ms, char* out, size_t size) {
    if (!ms) return unavailable(out, size);
    snprintf(out, size, "%.0f", *ms);
}

void percent(const std::optional<float>& fraction, char* out, size_t size) {
    if (!fraction) return unavailable(out, size);
    const float p = *fraction * 100.0f;
    if (p > 0.0f && p < 0.05f) {
        snprintf(out, size, "<0.1");
    } else if (p < 9.95f) {
        snprintf(out, size, "%.1f", p);
    } else {
        snprintf(out, size, "%.0f", p);
    }
}

void duration(uint64_t s, char* out, size_t size) {
    const unsigned long d = s / 86400, h = s / 3600 % 24, m = s / 60 % 60, sec = s % 60;
    if (d) {
        snprintf(out, size, "%lud %luh", d, h);
    } else if (h) {
        snprintf(out, size, "%luh %02lum", h, m);
    } else if (m) {
        snprintf(out, size, "%lum %02lus", m, sec);
    } else {
        snprintf(out, size, "%lus", sec);
    }
}

void duration(const std::optional<uint64_t>& seconds, char* out, size_t size) {
    if (!seconds) return unavailable(out, size);
    duration(*seconds, out, size);
}

}  // namespace ui::fmt
