#pragma once

#include <lvgl.h>

#include "../starlink/StarlinkSnapshot.h"

namespace ui {

// Main screen: overall state, download/upload, latency, obstruction,
// signal and uptime. Shows a status panel instead of numbers whenever
// there is no fresh data (connecting, dish unreachable, WiFi lost).
class DashboardScreen {
public:
    using Callback = void (*)(void* ctx);

    // onLongPress: opens the hardware test screen.
    void build(Callback onLongPress, void* ctx);
    void update(const starlink::StarlinkSnapshot& s, uint32_t nowMs);
    lv_obj_t* screen() const { return _screen; }

private:
    struct Metric {
        lv_obj_t* box;
        lv_obj_t* value;
        lv_obj_t* unit;
        lv_obj_t* caption;
    };

    Metric makeMetric(lv_obj_t* parent, const lv_font_t* valueFont, const char* caption, uint32_t accent);
    Metric makeTile(lv_obj_t* parent, const char* caption);
    void applyLayout(bool landscape);
    starlink::Health holdDegraded(const starlink::Health& h, uint32_t now);
    void showHealth(const starlink::Health& h, bool panelVisible);
    void showMetrics(const starlink::StarlinkStatus& st);
    void showPanel(const starlink::StarlinkSnapshot& s, const starlink::Health& h, uint32_t now);

    static void onLongPressed(lv_event_t* e);

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _badge = nullptr;
    lv_obj_t* _badgeDot = nullptr;
    lv_obj_t* _badgeText = nullptr;
    lv_obj_t* _reason = nullptr;
    lv_obj_t* _body = nullptr;
    lv_obj_t* _primary = nullptr;
    lv_obj_t* _grid = nullptr;
    lv_obj_t* _panel = nullptr;
    lv_obj_t* _panelTitle = nullptr;
    lv_obj_t* _panelReason = nullptr;
    lv_obj_t* _panelDetail = nullptr;
    lv_obj_t* _panelHint = nullptr;

    Metric _download{}, _upload{};
    Metric _latency{}, _obstruction{}, _signal{}, _uptime{};

    Callback _onLongPress = nullptr;
    void* _ctx = nullptr;
    bool _landscape = false;
    bool _layoutApplied = false;

    // DEGRADED is held for a while after the last degraded sample so a
    // single noisy poll doesn't make the badge flicker.
    starlink::Health _held;
    uint32_t _holdUntil = 0;
    starlink::HealthState _shownState = starlink::HealthState::Connecting;
    bool _shownAny = false;
};

}  // namespace ui
