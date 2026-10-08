#include "HistoryScreen.h"

#include <stdio.h>
#include <string.h>

#include "Format.h"
#include "Theme.h"

namespace ui {

using namespace theme;
using starlink::Series;

namespace {

constexpr float kThroughputFloorBps = 1e5f;  // axis never below 100 kbps
constexpr float kLatencyFloorMs = 50.0f;

void setText(lv_obj_t* label, const char* text) {
    if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, text);
    return l;
}

lv_obj_t* container(lv_obj_t* parent, lv_flex_flow_t flow) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_remove_style_all(c);
    lv_obj_set_flex_flow(c, flow);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);  // let swipes reach the screen
    return c;
}

// Caption on the left, current values on the right. The row wraps: values
// that don't fit beside the caption move to a second line instead of
// clipping it. (Throughput values always take the second line in portrait,
// see applyLayout().)
lv_obj_t* captionRow(lv_obj_t* parent, const char* caption) {
    lv_obj_t* row = container(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(row, 2, 0);
    label(row, &lv_font_montserrat_12, kMuted, caption);
    return row;
}

// The values, kept together so they wrap as one unit.
lv_obj_t* valuesGroup(lv_obj_t* row) {
    lv_obj_t* g = container(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(g, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_column(g, 8, 0);
    return g;
}

// "46 kbps" from an optional value.
void throughputText(const std::optional<float>& bps, const char* prefix, char* out, size_t size) {
    char num[16];
    const char* unit;
    fmt::throughput(bps, num, sizeof(num), &unit);
    snprintf(out, size, "%s %s %s", prefix, num, bps ? unit : "");
}

}  // namespace

void HistoryScreen::build() {
    _screen = lv_obj_create(nullptr);
    // Screens are built on demand and deleted when not shown (saves heap);
    // drop every pointer into the LVGL tree when that happens.
    lv_obj_add_event_cb(
        _screen, [](lv_event_t* e) { static_cast<HistoryScreen*>(lv_event_get_user_data(e))->onDeleted(); },
        LV_EVENT_DELETE, this);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_screen, 10, 0);
    lv_obj_set_style_pad_bottom(_screen, 14, 0);  // room for the page dots
    lv_obj_set_style_pad_row(_screen, 4, 0);
    lv_obj_set_flex_flow(_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* header = container(_screen, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* title = label(header, &lv_font_montserrat_14, kText, "HISTORY");
    lv_obj_set_style_text_letter_space(title, 3, 0);
    char window[16];
    snprintf(window, sizeof(window), "last %lu min", (unsigned long)(starlink::HistoryBuffer::kWindowS / 60));
    label(header, &lv_font_montserrat_12, kMuted, window);

    // Throughput: download + upload on one axis; current values as legend.
    lv_obj_t* row = captionRow(_screen, "THROUGHPUT");
    _throughputValues = valuesGroup(row);
    _down = label(_throughputValues, &lv_font_montserrat_12, kAccentDown, "");
    _up = label(_throughputValues, &lv_font_montserrat_12, kAccentUp, "");
    _throughputPlot.create(_screen);
    lv_obj_set_width(_throughputPlot.obj(), LV_PCT(100));
    lv_obj_set_flex_grow(_throughputPlot.obj(), 3);

    row = captionRow(_screen, "LATENCY");
    lv_obj_set_style_pad_top(row, 2, 0);
    _latency = label(row, &lv_font_montserrat_12, kText, "");
    _latencyPlot.create(_screen);
    lv_obj_set_width(_latencyPlot.obj(), LV_PCT(100));
    lv_obj_set_flex_grow(_latencyPlot.obj(), 2);

    // Time axis under the last plot.
    lv_obj_t* axis = container(_screen, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(axis, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(axis, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    snprintf(window, sizeof(window), "-%lu min", (unsigned long)(starlink::HistoryBuffer::kWindowS / 60));
    label(axis, &lv_font_montserrat_12, kMuted, window);
    label(axis, &lv_font_montserrat_12, kMuted, "now");

    applyLayout();
    refresh();
}

void HistoryScreen::applyLayout() {
    const int landscape =
        lv_display_get_horizontal_resolution(nullptr) > lv_display_get_vertical_resolution(nullptr) ? 1 : 0;
    if (landscape == _layoutLandscape) return;
    _layoutLandscape = landscape;
    // Portrait: down/up speeds always on their own line under "THROUGHPUT"
    // (a full-width group forces the wrap). Landscape: beside the caption.
    lv_obj_set_width(_throughputValues, landscape ? LV_SIZE_CONTENT : LV_PCT(100));
}

void HistoryScreen::update(const starlink::StarlinkService& service, uint32_t nowMs) {
    if (built()) applyLayout();
    // The buffer changes twice per 2 s slot (time advances, then the poll
    // lands); redrawing both plots costs ~60-80 ms, so refresh at most once
    // per slot. The copy is kept while hidden, so the page is complete when
    // shown.
    if (nowMs - _lastRefreshMs < starlink::HistoryBuffer::kPeriodMs - 100) return;
    if (service.copyHistory(_data, _version)) {
        _lastRefreshMs = nowMs;
        if (built()) refresh();
    }
}

void HistoryScreen::refresh() {
    const starlink::SeriesStats down = _data.stats(Series::Down);
    const starlink::SeriesStats up = _data.stats(Series::Up);
    const starlink::SeriesStats lat = _data.stats(Series::Latency);

    char buf[40];
    throughputText(down.latest, LV_SYMBOL_DOWN, buf, sizeof(buf));
    setText(_down, buf);
    throughputText(up.latest, LV_SYMBOL_UP, buf, sizeof(buf));
    setText(_up, buf);
    if (lat.latest && lat.mean) {
        snprintf(buf, sizeof(buf), "now %.0f  avg %.0f ms", *lat.latest, *lat.mean);
    } else {
        snprintf(buf, sizeof(buf), "%s", fmt::kUnavailable);
    }
    setText(_latency, buf);

    // Axes: round the window's peak up to 1/2/5 x 10^n.
    const float peak = LV_MAX(down.max.value_or(0.0f), up.max.value_or(0.0f));
    const float tMax = fmt::niceCeil(peak, kThroughputFloorBps);
    char num[16];
    const char* unit;
    fmt::throughput(tMax, num, sizeof(num), &unit);
    snprintf(buf, sizeof(buf), "%s %s", num, unit);
    const TimeSeriesPlot::Line tLines[] = {{Series::Down, kAccentDown}, {Series::Up, kAccentUp}};
    _throughputPlot.set(&_data, tLines, 2, tMax, buf);

    const float lMax = fmt::niceCeil(lat.max.value_or(0.0f), kLatencyFloorMs);
    snprintf(buf, sizeof(buf), "%.0f ms", lMax);
    const TimeSeriesPlot::Line lLines[] = {{Series::Latency, kOk}};
    _latencyPlot.set(&_data, lLines, 1, lMax, buf);
}

}  // namespace ui
