#include "AlignmentMath.h"

#include <math.h>

namespace ui::align {

namespace {
constexpr float kDegToRad = 3.14159265358979f / 180.0f;
}

float normalize360(float deg) {
    float d = fmodf(deg, 360.0f);
    if (d < 0) d += 360.0f;
    if (d >= 360.0f) d -= 360.0f;  // fmodf(-1e-6) + 360 can round to 360
    return d;
}

float wrap180(float deg) {
    const float d = normalize360(deg);
    return d > 180.0f ? d - 360.0f : d;
}

const char* cardinal16(float azimuthDeg) {
    static const char* const kNames[] = {"N",  "NNE", "NE", "ENE", "E",  "ESE", "SE", "SSE",
                                         "S",  "SSW", "SW", "WSW", "W",  "WNW", "NW", "NNW"};
    const int i = static_cast<int>(normalize360(azimuthDeg) / 22.5f + 0.5f) % 16;
    return kNames[i];
}

Offset offset(const std::optional<float>& az, const std::optional<float>& el,
              const std::optional<float>& desiredAz, const std::optional<float>& desiredEl) {
    Offset o;
    if (az && desiredAz) o.azimuth = wrap180(*desiredAz - *az);
    if (el && desiredEl) o.elevation = *desiredEl - *el;
    return o;
}

PlotPoint project(float azimuthDeg, float elevationDeg, float maxZenithDeg) {
    float r = (90.0f - elevationDeg) / maxZenithDeg;
    if (r < 0) r = 0;  // elevation > 90 is not physical; treat as straight up
    const bool clamped = r > 1.0f;
    if (clamped) r = 1.0f;
    const float a = azimuthDeg * kDegToRad;
    return {r * sinf(a), -r * cosf(a), clamped};
}

}  // namespace ui::align
