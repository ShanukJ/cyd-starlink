#include "DashboardScreen.h"

#include <stdio.h>
#include <string.h>

#include "Format.h"
#include "Theme.h"
#include "fonts/Fonts.h"

namespace ui {

using starlink::Health;
using starlink::HealthState;
using namespace theme;

namespace {

constexpr uint32_t kDegradedHoldMs = 10000;

// Sets label text only when it changed: avoids needless redraws.
void setText(lv_obj_t* label, const char* text) {
    if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void setColor(lv_obj_t* obj, uint32_t color) {
    if (!lv_color_eq(lv_obj_get_style_text_color(obj, LV_PART_MAIN), lv_color_hex(color))) {
        lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    }
}

void setVisible(lv_obj_t* obj, bool visible) {
    if (visible == lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
        if (visible) {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
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
    lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);  // let presses reach the screen (long-press)
    return c;
}

struct StateLook {
    const char* text;
    uint32_t color;
};

// Symbol + word + colour: readable without relying on colour.
StateLook lookFor(HealthState s) {
    switch (s) {
        case HealthState::Online: return {"ONLINE", kOk};  // drawn dot, see showHealth()
        case HealthState::Degraded: return {LV_SYMBOL_WARNING " DEGRADED", kWarn};
        case HealthState::Offline: return {LV_SYMBOL_CLOSE " OFFLINE", kFail};
        case HealthState::Error: return {LV_SYMBOL_CLOSE " ERROR", kFail};
        case HealthState::Connecting: break;
    }
    return {LV_SYMBOL_REFRESH " CONNECTING", kMuted};
}

// Distance from a value's bottom to its font's baseline differs per size;
// nudging the unit down keeps "183 Mbps" on one visual baseline.
int32_t unitBaselinePad(const lv_font_t* valueFont) {
    return lv_font_get_line_height(valueFont) / 7;
}

}  // namespace

DashboardScreen::Metric DashboardScreen::makeMetric(lv_obj_t* parent, const lv_font_t* valueFont, const char* caption,
                                                    uint32_t accent) {
    Metric m{};
    m.box = container(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_size(m.box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(m.box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* row = container(m.box, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 6, 0);
    m.value = label(row, valueFont, kText, fmt::kUnavailable);
    m.unit = label(row, &lv_font_montserrat_14, kMuted, "");
    lv_obj_set_style_pad_bottom(m.unit, unitBaselinePad(valueFont), 0);

    m.caption = label(m.box, &lv_font_montserrat_12, accent, caption);
    lv_obj_set_style_text_letter_space(m.caption, 2, 0);
    return m;
}

DashboardScreen::Metric DashboardScreen::makeTile(lv_obj_t* parent, const char* caption) {
    Metric m{};
    m.box = container(parent, LV_FLEX_FLOW_COLUMN);
    // Content-sized; the grid stretches it. Without this the box keeps
    // LVGL's default ~140 px height and content-sized grid rows balloon.
    lv_obj_set_size(m.box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(m.box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(m.box, lv_color_hex(kCard), 0);
    lv_obj_set_style_bg_opa(m.box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(m.box, 8, 0);
    lv_obj_set_style_pad_ver(m.box, 6, 0);
    lv_obj_set_style_pad_hor(m.box, 4, 0);

    lv_obj_t* row = container(m.box, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(row, 4, 0);
    m.value = label(row, &lv_font_montserrat_28, kText, fmt::kUnavailable);
    m.unit = label(row, &lv_font_montserrat_14, kMuted, "");

    m.caption = label(m.box, &lv_font_montserrat_12, kMuted, caption);
    return m;
}

void DashboardScreen::build(Callback onLongPress, void* ctx) {
    _onLongPress = onLongPress;
    _ctx = ctx;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_screen, 10, 0);
    lv_obj_set_style_pad_row(_screen, 6, 0);
    lv_obj_set_flex_flow(_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(_screen, onLongPressed, LV_EVENT_LONG_PRESSED, this);

    // Header: name + state badge, then an optional reason line.
    lv_obj_t* header = container(_screen, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* title = label(header, &lv_font_montserrat_14, kText, "STARLINK");
    lv_obj_set_style_text_letter_space(title, 3, 0);
    // Badge: tinted pill with [dot] + text. The dot is drawn (the font's
    // bullet glyph is too small to read as a status light).
    _badge = container(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(_badge, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_align(_badge, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(_badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(_badge, LV_OPA_20, 0);
    lv_obj_set_style_pad_hor(_badge, 8, 0);
    lv_obj_set_style_pad_ver(_badge, 3, 0);
    lv_obj_set_style_pad_column(_badge, 6, 0);
    _badgeDot = lv_obj_create(_badge);
    lv_obj_remove_style_all(_badgeDot);
    lv_obj_set_size(_badgeDot, 8, 8);
    lv_obj_set_style_radius(_badgeDot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(_badgeDot, LV_OPA_COVER, 0);
    _badgeText = label(_badge, &lv_font_montserrat_14, kMuted, "");

    _reason = label(_screen, &lv_font_montserrat_12, kWarn, "");
    lv_obj_set_width(_reason, LV_PCT(100));
    lv_obj_set_style_text_align(_reason, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_add_flag(_reason, LV_OBJ_FLAG_HIDDEN);

    // Body: primary numbers + 2x2 tiles. Arranged by applyLayout().
    _body = container(_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(_body, LV_PCT(100));
    lv_obj_set_flex_grow(_body, 1);

    _primary = container(_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_primary, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _download = makeMetric(_primary, &sm_font_num_48, LV_SYMBOL_DOWN " DOWNLOAD", kAccentDown);
    _upload = makeMetric(_primary, &sm_font_num_36, LV_SYMBOL_UP " UPLOAD", kAccentUp);

    _grid = lv_obj_create(_body);
    lv_obj_remove_style_all(_grid);
    lv_obj_remove_flag(_grid, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_row(_grid, 6, 0);
    lv_obj_set_style_pad_column(_grid, 6, 0);
    _latency = makeTile(_grid, "LATENCY");
    _obstruction = makeTile(_grid, "OBSTRUCTION");
    _signal = makeTile(_grid, "SIGNAL");
    _uptime = makeTile(_grid, "UPTIME");
    setText(_latency.unit, "ms");
    setText(_obstruction.unit, "%");
    setText(_signal.unit, "%");

    // Status panel, shown instead of the body when there is no fresh data.
    _panel = container(_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(_panel, LV_PCT(100));
    lv_obj_set_flex_grow(_panel, 1);
    lv_obj_set_flex_align(_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(_panel, 8, 0);
    _panelTitle = label(_panel, &lv_font_montserrat_28, kMuted, "");
    _panelReason = label(_panel, &lv_font_montserrat_20, kText, "");
    _panelDetail = label(_panel, &lv_font_montserrat_14, kMuted, "");
    _panelHint = label(_panel, &lv_font_montserrat_14, kMuted, "");
    for (lv_obj_t* l : {_panelTitle, _panelReason, _panelDetail, _panelHint}) {
        lv_obj_set_width(l, LV_PCT(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    }
    lv_obj_add_flag(_panel, LV_OBJ_FLAG_HIDDEN);
}

void DashboardScreen::applyLayout(bool landscape) {
    if (_layoutApplied && landscape == _landscape) return;
    _layoutApplied = true;
    _landscape = landscape;

    // Portrait 240x320: numbers on top, 2x2 tiles below.
    // Landscape 320x240: numbers on the left, tiles as a 1x4 list on the
    // right (a 2x2 grid would leave ~70 px tiles: too narrow for captions).
    static const int32_t cols2[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows2[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static const int32_t cols1[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const int32_t rows4[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1),
                                    LV_GRID_TEMPLATE_LAST};
    Metric* tiles[] = {&_latency, &_obstruction, &_signal, &_uptime};

    if (landscape) {
        lv_obj_set_flex_flow(_body, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(_body, 8, 0);
        lv_obj_set_size(_primary, LV_PCT(45), LV_PCT(100));
        lv_obj_set_flex_grow(_primary, 0);
        lv_obj_set_flex_grow(_grid, 1);
        lv_obj_set_height(_grid, LV_PCT(100));
        lv_obj_set_grid_dsc_array(_grid, cols1, rows4);
    } else {
        lv_obj_set_flex_flow(_body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(_body, 8, 0);
        // Reset every size the landscape layout set, or rotating back
        // leaves the numbers squeezed out by a full-height grid.
        lv_obj_set_size(_primary, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(_primary, 1);
        lv_obj_set_flex_grow(_grid, 0);
        lv_obj_set_size(_grid, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_grid_dsc_array(_grid, cols2, rows2);
    }

    const lv_font_t* tileFont = landscape ? &lv_font_montserrat_20 : &lv_font_montserrat_28;
    for (int i = 0; i < 4; ++i) {
        Metric* t = tiles[i];
        const int col = landscape ? 0 : i % 2;
        const int row = landscape ? i : i / 2;
        lv_obj_set_grid_cell(t->box, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
        // Portrait: value above caption. Landscape: caption left, value right.
        lv_obj_set_flex_flow(t->box, landscape ? LV_FLEX_FLOW_ROW_REVERSE : LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(t->box, landscape ? LV_FLEX_ALIGN_SPACE_BETWEEN : LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(t->box, landscape ? 8 : 4, 0);
        lv_obj_set_style_text_font(t->value, tileFont, 0);
        lv_obj_set_style_pad_bottom(t->unit, unitBaselinePad(tileFont), 0);
    }
}

Health DashboardScreen::holdDegraded(const Health& h, uint32_t now) {
    if (h.state == HealthState::Degraded) {
        _held = h;
        _holdUntil = now + kDegradedHoldMs;
        return h;
    }
    if (h.state == HealthState::Online && _held.state == HealthState::Degraded &&
        static_cast<int32_t>(now - _holdUntil) < 0) {
        return _held;
    }
    _held = h;
    return h;
}

void DashboardScreen::showHealth(const Health& h, bool panelVisible) {
    const StateLook look = lookFor(h.state);
    if (!_shownAny || h.state != _shownState) {
        _shownAny = true;
        _shownState = h.state;
        lv_label_set_text(_badgeText, look.text);
        lv_obj_set_style_text_color(_badgeText, lv_color_hex(look.color), 0);
        lv_obj_set_style_bg_color(_badge, lv_color_hex(look.color), 0);
        lv_obj_set_style_bg_color(_badgeDot, lv_color_hex(look.color), 0);
        setVisible(_badgeDot, h.state == HealthState::Online);
    }

    // The status panel repeats the reason in large type; don't show it twice.
    const bool showReason = !panelVisible && h.reason[0] && h.state != HealthState::Online;
    setVisible(_reason, showReason);
    if (showReason) {
        setText(_reason, h.reason);
        setColor(_reason, look.color);
    }
}

void DashboardScreen::showMetrics(const starlink::StarlinkStatus& st) {
    char buf[16];
    const char* unit;

    fmt::throughput(st.downlinkBps, buf, sizeof(buf), &unit);
    setText(_download.value, buf);
    setText(_download.unit, unit);
    fmt::throughput(st.uplinkBps, buf, sizeof(buf), &unit);
    setText(_upload.value, buf);
    setText(_upload.unit, unit);

    fmt::latency(st.popPingLatencyMs, buf, sizeof(buf));
    setText(_latency.value, buf);
    fmt::percent(st.fractionObstructed, buf, sizeof(buf));
    setText(_obstruction.value, buf);
    fmt::percent(st.signalQuality, buf, sizeof(buf));
    setText(_signal.value, buf);
    fmt::duration(st.uptimeS, buf, sizeof(buf));
    setText(_uptime.value, buf);

    // Units only make sense next to a number.
    setVisible(_latency.unit, st.popPingLatencyMs.has_value());
    setVisible(_obstruction.unit, st.fractionObstructed.has_value());
    setVisible(_signal.unit, st.signalQuality.has_value());
}

void DashboardScreen::showPanel(const starlink::StarlinkSnapshot& s, const Health& h, uint32_t now) {
    const StateLook look = lookFor(h.state);
    setText(_panelTitle, look.text);
    setColor(_panelTitle, look.color);
    setText(_panelReason, h.reason);

    char buf[48];
    if (s.hasStatus) {
        char ago[16];
        fmt::duration(static_cast<uint64_t>((now - s.lastOkMs) / 1000), ago, sizeof(ago));
        snprintf(buf, sizeof(buf), "Last seen %s ago", ago);
    } else {
        snprintf(buf, sizeof(buf), "Dish %s", s.host[0] ? s.host : "--");
    }
    setText(_panelDetail, buf);

    const char* hint = "";
    if (s.state == starlink::LinkState::Unreachable) {
        hint = h.state == HealthState::Error ? "Check the dish address in WiFi setup" : "Retrying...";
    }
    setText(_panelHint, hint);
}

void DashboardScreen::update(const starlink::StarlinkSnapshot& s, uint32_t nowMs) {
    applyLayout(lv_display_get_horizontal_resolution(nullptr) > lv_display_get_vertical_resolution(nullptr));

    const Health h = holdDegraded(s.health, nowMs);
    // Numbers only while the data is fresh; otherwise the status panel.
    const bool fresh = s.state == starlink::LinkState::Online && s.hasStatus;
    showHealth(h, !fresh);
    // The panel states the condition in large type; the badge would repeat it.
    setVisible(_badge, fresh);

    setVisible(_body, fresh);
    setVisible(_panel, !fresh);
    if (fresh) {
        showMetrics(s.status);
    } else {
        showPanel(s, h, nowMs);
    }
}

void DashboardScreen::onLongPressed(lv_event_t* e) {
    auto* self = static_cast<DashboardScreen*>(lv_event_get_user_data(e));
    if (self->_onLongPress) self->_onLongPress(self->_ctx);
}

}  // namespace ui
