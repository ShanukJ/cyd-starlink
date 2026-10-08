#include "SkyPlot.h"

#include <math.h>

#include "AlignmentMath.h"
#include "Theme.h"

namespace ui {

using namespace theme;

namespace {

constexpr int32_t kLabelMargin = 14;  // room for N/E/S/W outside the circle
constexpr int32_t kDotRadius = 6;
constexpr int32_t kTargetRadius = 8;

bool samePointing(const std::optional<SkyPlot::Pointing>& a, const std::optional<SkyPlot::Pointing>& b) {
    if (a.has_value() != b.has_value()) return false;
    if (!a) return true;
    // 0.05 deg is far below one pixel on this plot.
    return fabsf(a->azimuthDeg - b->azimuthDeg) < 0.05f && fabsf(a->elevationDeg - b->elevationDeg) < 0.05f;
}

void circle(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t r, uint32_t color, int32_t width) {
    lv_draw_arc_dsc_t d;
    lv_draw_arc_dsc_init(&d);
    d.center.x = cx;
    d.center.y = cy;
    d.radius = r;
    d.start_angle = 0;
    d.end_angle = 360;
    d.width = width;
    d.color = lv_color_hex(color);
    lv_draw_arc(layer, &d);
}

void line(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color, int32_t dash) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1.x = x1;
    d.p1.y = y1;
    d.p2.x = x2;
    d.p2.y = y2;
    d.width = 1;
    d.dash_width = dash;
    d.dash_gap = dash;
    d.color = lv_color_hex(color);
    lv_draw_line(layer, &d);
}

void dot(lv_layer_t* layer, int32_t x, int32_t y, int32_t r, uint32_t color) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = LV_RADIUS_CIRCLE;
    d.bg_color = lv_color_hex(color);
    d.bg_opa = LV_OPA_COVER;
    d.border_width = 2;
    d.border_color = lv_color_hex(kBg);  // separates the dot from lines under it
    const lv_area_t a = {x - r, y - r, x + r, y + r};
    lv_draw_rect(layer, &d, &a);
}

void text(lv_layer_t* layer, int32_t cx, int32_t cy, const char* s, uint32_t color) {
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.font = &lv_font_montserrat_12;
    d.color = lv_color_hex(color);
    d.text = s;  // string literal: outlives the deferred draw
    d.align = LV_TEXT_ALIGN_CENTER;
    const lv_area_t a = {cx - 10, cy - 7, cx + 10, cy + 7};
    lv_draw_label(layer, &d, &a);
}

}  // namespace

lv_obj_t* SkyPlot::create(lv_obj_t* parent) {
    _obj = lv_obj_create(parent);
    lv_obj_remove_style_all(_obj);
    lv_obj_remove_flag(_obj, LV_OBJ_FLAG_CLICKABLE);  // let swipes reach the screen
    lv_obj_remove_flag(_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(_obj, onDraw, LV_EVENT_DRAW_MAIN, this);
    return _obj;
}

void SkyPlot::set(const std::optional<Pointing>& current, const std::optional<Pointing>& desired) {
    if (samePointing(current, _current) && samePointing(desired, _desired)) return;
    _current = current;
    _desired = desired;
    lv_obj_invalidate(_obj);
}

void SkyPlot::onDraw(lv_event_t* e) {
    auto* self = static_cast<SkyPlot*>(lv_event_get_user_data(e));
    lv_area_t area;
    lv_obj_get_content_coords(self->_obj, &area);
    self->draw(lv_event_get_layer(e), area);
}

void SkyPlot::draw(lv_layer_t* layer, const lv_area_t& area) const {
    const int32_t cx = (area.x1 + area.x2) / 2;
    const int32_t cy = (area.y1 + area.y2) / 2;
    const int32_t R = LV_MIN(lv_area_get_width(&area), lv_area_get_height(&area)) / 2 - kLabelMargin;
    if (R < 20) return;

    // Rings every 10 degrees from vertical, cross-hair, compass labels.
    for (int i = 1; i <= 3; ++i) circle(layer, cx, cy, R * i / 3, i == 3 ? kMuted : kDivider, 1);
    line(layer, cx - R, cy, cx + R, cy, kDivider, 0);
    line(layer, cx, cy - R, cx, cy + R, kDivider, 0);
    text(layer, cx, cy - R - 8, "N", kText);
    text(layer, cx + R + 8, cy, "E", kMuted);
    text(layer, cx, cy + R + 8, "S", kMuted);
    text(layer, cx - R - 8, cy, "W", kMuted);

    auto toScreen = [&](const Pointing& p, int32_t& x, int32_t& y) {
        const align::PlotPoint pp = align::project(p.azimuthDeg, p.elevationDeg, kMaxZenithDeg);
        x = cx + static_cast<int32_t>(pp.x * R);
        y = cy + static_cast<int32_t>(pp.y * R);
    };

    int32_t dx = 0, dy = 0, tx = 0, ty = 0;
    if (_current) toScreen(*_current, dx, dy);
    if (_desired) toScreen(*_desired, tx, ty);
    if (_current && _desired) line(layer, dx, dy, tx, ty, kMuted, 3);
    if (_desired) circle(layer, tx, ty, kTargetRadius, kOk, 2);
    if (_current) dot(layer, dx, dy, kDotRadius, kAccentDown);
}

}  // namespace ui
