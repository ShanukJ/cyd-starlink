#pragma once

#include <lvgl.h>

#include "../starlink/StarlinkSnapshot.h"
#include "SkyPlot.h"

namespace ui {

// Dish pointing: current vs Starlink's desired azimuth/elevation, tilt,
// attitude state and uncertainty, a top-down plot, and plain guidance
// ("Turn 14° clockwise"). Makes no claim about what counts as "aligned":
// Starlink publishes no tolerance.
class AlignmentScreen {
public:
    void build();
    void update(const starlink::StarlinkSnapshot& s);
    lv_obj_t* screen() const { return _screen; }

private:
    void applyLayout(bool landscape);

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _attitude = nullptr;
    lv_obj_t* _body = nullptr;
    lv_obj_t* _side = nullptr;
    lv_obj_t* _table = nullptr;
    static constexpr int kRows = 4;  // azimuth, elevation, tilt, uncertainty
    lv_obj_t* _rowLabel[kRows] = {};
    lv_obj_t* _cur[kRows] = {};
    lv_obj_t* _want[kRows] = {};
    lv_obj_t* _guide = nullptr;
    SkyPlot _plot;

    bool _landscape = false;
    bool _layoutApplied = false;
};

}  // namespace ui
