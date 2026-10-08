#pragma once

#include <lvgl.h>

#include "../starlink/HistoryBuffer.h"

namespace ui {

// Line graph of up to two series from a HistoryBuffer (oldest left, now
// right). One point per pixel column (the mean of the slots it covers);
// empty slots break the line, so gaps are visible. Drawn directly into the
// LVGL layer: no canvas buffer.
class TimeSeriesPlot {
public:
    struct Line {
        starlink::Series series;
        uint32_t color;
    };

    lv_obj_t* create(lv_obj_t* parent);
    // `data` must outlive the plot. yMax: top of the axis in the series'
    // units; yLabel: text drawn at the top-left (e.g. "200 Mbps").
    void set(const starlink::HistoryBuffer* data, const Line* lines, int lineCount, float yMax, const char* yLabel);
    void invalidate() { lv_obj_invalidate(_obj); }
    lv_obj_t* obj() const { return _obj; }

private:
    static constexpr int kMaxLines = 2;

    static void onDraw(lv_event_t* e);
    void draw(lv_layer_t* layer, const lv_area_t& area) const;

    lv_obj_t* _obj = nullptr;
    const starlink::HistoryBuffer* _data = nullptr;
    Line _lines[kMaxLines] = {};
    int _lineCount = 0;
    float _yMax = 1.0f;
    char _yLabel[16] = "";
};

}  // namespace ui
