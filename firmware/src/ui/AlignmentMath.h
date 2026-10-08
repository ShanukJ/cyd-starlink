#pragma once

// Geometry for the alignment screen. Pure C++ (host-tested in
// test/test_alignment).
//
// Conventions (same as the dish): azimuth in degrees clockwise from true
// north, elevation in degrees above the horizon. The plot is a top-down
// "map" view: north up, east right, centre = pointing straight up.

#include <optional>

namespace ui::align {

float normalize360(float deg);  // -> [0, 360)
float wrap180(float deg);       // -> (-180, 180]

// 16-point compass name: "N", "NNE", ... "NNW".
const char* cardinal16(float azimuthDeg);

struct Offset {
    std::optional<float> azimuth;    // desired - current, wrapped; + = turn clockwise
    std::optional<float> elevation;  // desired - current; + = raise
};
Offset offset(const std::optional<float>& az, const std::optional<float>& el,
              const std::optional<float>& desiredAz, const std::optional<float>& desiredEl);

// Position of a pointing direction on the plot, in units of the plot
// radius (x right, y down). `maxZenithDeg` is the angle from vertical at
// the plot edge; directions beyond it are clamped to the edge.
struct PlotPoint {
    float x;
    float y;
    bool clamped;
};
PlotPoint project(float azimuthDeg, float elevationDeg, float maxZenithDeg);

}  // namespace ui::align
