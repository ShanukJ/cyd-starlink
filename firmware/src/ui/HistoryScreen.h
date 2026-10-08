#pragma once

#include <lvgl.h>

#include "../starlink/HistoryBuffer.h"
#include "../starlink/StarlinkService.h"
#include "TimeSeriesPlot.h"

namespace ui {

// Last 15 minutes: throughput (download + upload) and latency graphs with
// current values and auto-scaled axes.
class HistoryScreen {
public:
    void build();
    void update(const starlink::StarlinkService& service, uint32_t nowMs);
    lv_obj_t* screen() const { return _screen; }
    bool built() const { return _screen != nullptr; }

private:
    void refresh();
    void applyLayout();
    void onDeleted() {
        _screen = nullptr;
        _layoutLandscape = -1;
    }

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _throughputValues = nullptr;
    lv_obj_t* _down = nullptr;
    lv_obj_t* _up = nullptr;
    lv_obj_t* _latency = nullptr;
    TimeSeriesPlot _throughputPlot;
    TimeSeriesPlot _latencyPlot;

    starlink::HistoryBuffer _data;  // UI-side copy, refreshed when the service's changes
    uint32_t _version = UINT32_MAX;
    uint32_t _lastRefreshMs = 0;
    int _layoutLandscape = -1;  // -1 = not applied yet
};

}  // namespace ui
