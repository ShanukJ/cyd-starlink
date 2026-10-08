#pragma once

#include <lvgl.h>

#include <optional>

namespace ui {

// Top-down pointing plot for the alignment screen: north up, east right,
// centre = straight up, edge = kMaxZenithDeg from vertical. Draws the
// dish's current pointing (filled dot) and Starlink's desired pointing
// (ring) with a line between them. Drawn directly into the LVGL layer:
// no canvas buffer.
class SkyPlot {
public:
    static constexpr float kMaxZenithDeg = 30.0f;  // Starlink dishes sit well within this

    struct Pointing {
        float azimuthDeg;
        float elevationDeg;
    };

    lv_obj_t* create(lv_obj_t* parent);
    void set(const std::optional<Pointing>& current, const std::optional<Pointing>& desired);
    lv_obj_t* obj() const { return _obj; }

private:
    static void onDraw(lv_event_t* e);
    void draw(lv_layer_t* layer, const lv_area_t& area) const;

    lv_obj_t* _obj = nullptr;
    std::optional<Pointing> _current;
    std::optional<Pointing> _desired;
};

}  // namespace ui
