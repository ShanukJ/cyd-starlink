#include "TimeSeriesPlot.h"

#include <math.h>
#include <string.h>

#include "Theme.h"

namespace ui {

using namespace theme;
using starlink::HistoryBuffer;

namespace {

// LVGL renders in horizontal bands and calls the draw handler once per
// band; segments outside the band being drawn are skipped. `_clip_area` is
// LVGL-internal (stable across 9.x) and only used for this culling.
bool inBand(const lv_layer_t* layer, int32_t y1, int32_t y2) {
    const lv_area_t& clip = layer->_clip_area;
    return LV_MAX(y1, y2) >= clip.y1 && LV_MIN(y1, y2) <= clip.y2;
}

void hline(lv_layer_t* layer, int32_t x1, int32_t x2, int32_t y, uint32_t color, int32_t dash) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1.x = x1;
    d.p1.y = y;
    d.p2.x = x2;
    d.p2.y = y;
    d.width = 1;
    d.dash_width = dash;
    d.dash_gap = dash;
    d.color = lv_color_hex(color);
    lv_draw_line(layer, &d);
}

void text(lv_layer_t* layer, const lv_area_t& a, const char* s, uint32_t color, lv_text_align_t align) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = &lv_font_montserrat_12;
    d.color = lv_color_hex(color);
    d.text = s;
    d.text_local = 1;  // copy: the buffer may change before the deferred draw
    d.align = align;
    lv_draw_label(layer, &d, &a);
}

}  // namespace

lv_obj_t* TimeSeriesPlot::create(lv_obj_t* parent) {
    _obj = lv_obj_create(parent);
    lv_obj_remove_style_all(_obj);
    lv_obj_remove_flag(_obj, LV_OBJ_FLAG_CLICKABLE);  // let swipes reach the screen
    lv_obj_remove_flag(_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(_obj, lv_color_hex(kCard), 0);
    lv_obj_set_style_bg_opa(_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(_obj, 6, 0);
    lv_obj_set_style_pad_all(_obj, 4, 0);
    lv_obj_add_event_cb(_obj, onDraw, LV_EVENT_DRAW_MAIN, this);
    return _obj;
}

void TimeSeriesPlot::set(const HistoryBuffer* data, const Line* lines, int lineCount, float yMax, const char* yLabel) {
    _data = data;
    _lineCount = lineCount < kMaxLines ? lineCount : kMaxLines;
    for (int i = 0; i < _lineCount; ++i) _lines[i] = lines[i];
    _yMax = yMax > 0 ? yMax : 1.0f;
    strlcpy(_yLabel, yLabel, sizeof(_yLabel));
    lv_obj_invalidate(_obj);
}

void TimeSeriesPlot::onDraw(lv_event_t* e) {
    auto* self = static_cast<TimeSeriesPlot*>(lv_event_get_user_data(e));
    lv_area_t area;
    lv_obj_get_content_coords(self->_obj, &area);
    self->draw(lv_event_get_layer(e), area);
}

void TimeSeriesPlot::draw(lv_layer_t* layer, const lv_area_t& area) const {
    const int32_t w = lv_area_get_width(&area);
    const int32_t h = lv_area_get_height(&area);
    if (w < 8 || h < 8) return;

    // Grid: dashed top and middle, solid baseline. Scale label top-left.
    hline(layer, area.x1, area.x2, area.y1, kDivider, 2);
    hline(layer, area.x1, area.x2, area.y1 + h / 2, kDivider, 2);
    hline(layer, area.x1, area.x2, area.y2, kMuted, 0);
    const lv_area_t labelArea = {area.x1 + 2, area.y1 + 1, area.x1 + w / 2, area.y1 + 15};
    text(layer, labelArea, _yLabel, kMuted, LV_TEXT_ALIGN_LEFT);

    if (!_data) return;
    const size_t n = HistoryBuffer::kSlots;
    bool any = false;

    for (int li = 0; li < _lineCount; ++li) {
        const Line& line = _lines[li];
        // Columns are 1 px apart, so a 2 px wide vertical bar from the
        // previous point to this one draws a connected line. Axis-aligned
        // fills are LVGL's fastest path; anti-aliased diagonal lines were
        // ~10x slower (130+ ms per refresh on this ESP32).
        lv_draw_rect_dsc_t d;
        lv_draw_rect_dsc_init(&d);
        d.bg_color = lv_color_hex(line.color);
        d.bg_opa = LV_OPA_COVER;

        bool havePrev = false;
        int32_t py = 0;
        for (int32_t x = 0; x < w; ++x) {
            // Mean of the slots that fall into this pixel column.
            const size_t i0 = static_cast<size_t>(x) * n / w;
            size_t i1 = static_cast<size_t>(x + 1) * n / w;
            if (i1 <= i0) i1 = i0 + 1;
            float sum = 0;
            int cnt = 0;
            for (size_t i = i0; i < i1 && i < n; ++i) {
                const float v = HistoryBuffer::value(_data->at(i), line.series);
                if (isfinite(v)) {
                    sum += v;
                    cnt++;
                }
            }
            if (cnt == 0) {
                havePrev = false;  // gap: break the line
                continue;
            }
            any = true;
            float frac = (sum / cnt) / _yMax;
            if (frac > 1) frac = 1;
            if (frac < 0) frac = 0;
            const int32_t cx = area.x1 + x;
            const int32_t cy = area.y2 - static_cast<int32_t>(frac * (h - 1));
            if (inBand(layer, havePrev ? py : cy, cy)) {
                const int32_t top = havePrev ? LV_MIN(py, cy) : cy;
                const int32_t bottom = (havePrev ? LV_MAX(py, cy) : cy) + 1;  // >= 2 px tall when flat
                const lv_area_t bar = {cx - 1, top, cx, bottom};
                lv_draw_rect(layer, &d, &bar);
            }
            py = cy;
            havePrev = true;
        }
    }

    if (!any) {
        const lv_area_t mid = {area.x1, area.y1 + h / 2 - 7, area.x2, area.y1 + h / 2 + 7};
        text(layer, mid, "No data yet", kMuted, LV_TEXT_ALIGN_CENTER);
    }
}

}  // namespace ui
