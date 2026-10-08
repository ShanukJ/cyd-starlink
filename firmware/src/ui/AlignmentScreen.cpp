#include "AlignmentScreen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "AlignmentMath.h"
#include "Format.h"
#include "Theme.h"

namespace ui {

using namespace theme;
using starlink::AttitudeState;

namespace {

void setText(lv_obj_t* label, const char* text) {
    if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void setColor(lv_obj_t* obj, uint32_t color) {
    if (!lv_color_eq(lv_obj_get_style_text_color(obj, LV_PART_MAIN), lv_color_hex(color))) {
        lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
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
    lv_obj_remove_flag(c, LV_OBJ_FLAG_CLICKABLE);  // let swipes reach the screen
    return c;
}

// "346° NNW" (or "346°" when space is short)
void formatBearing(const std::optional<float>& az, bool withCardinal, char* out, size_t size) {
    if (!az) {
        snprintf(out, size, "%s", fmt::kUnavailable);
        return;
    }
    const float a = align::normalize360(*az);
    const float shown = roundf(a) >= 360.0f ? 0.0f : a;
    if (withCardinal) {
        snprintf(out, size, "%.0f\xC2\xB0 %s", shown, align::cardinal16(a));
    } else {
        snprintf(out, size, "%.0f\xC2\xB0", shown);
    }
}

void formatDegrees(const std::optional<float>& v, char* out, size_t size) {
    if (!v) {
        snprintf(out, size, "%s", fmt::kUnavailable);
        return;
    }
    snprintf(out, size, "%.1f\xC2\xB0", *v);
}

}  // namespace

void AlignmentScreen::build() {
    _screen = lv_obj_create(nullptr);
    // Screens are built on demand and deleted when not shown (saves heap);
    // drop every pointer into the LVGL tree when that happens.
    lv_obj_add_event_cb(
        _screen, [](lv_event_t* e) { static_cast<AlignmentScreen*>(lv_event_get_user_data(e))->onDeleted(); },
        LV_EVENT_DELETE, this);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_screen, 10, 0);
    lv_obj_set_style_pad_bottom(_screen, 14, 0);  // room for the page dots
    lv_obj_set_style_pad_row(_screen, 6, 0);
    lv_obj_set_flex_flow(_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(_screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* header = container(_screen, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t* title = label(header, &lv_font_montserrat_14, kText, "ALIGNMENT");
    lv_obj_set_style_text_letter_space(title, 3, 0);
    _attitude = label(header, &lv_font_montserrat_12, kMuted, fmt::kUnavailable);

    _body = container(_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(_body, LV_PCT(100));
    lv_obj_set_flex_grow(_body, 1);

    _plot.create(_body);

    _side = container(_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_side, 6, 0);
    lv_obj_set_flex_align(_side, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Table: row label | dish | target. Column headers double as the legend.
    static const int32_t cols[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    static const int32_t rows[] = {LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_CONTENT,
                                   LV_GRID_CONTENT, LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};
    _table = lv_obj_create(_side);
    lv_obj_remove_style_all(_table);
    lv_obj_remove_flag(_table, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(_table, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_grid_dsc_array(_table, cols, rows);
    lv_obj_set_style_pad_row(_table, 3, 0);
    lv_obj_set_style_pad_column(_table, 8, 0);

    auto place = [](lv_obj_t* o, int col, int row) {
        lv_obj_set_grid_cell(o, col == 0 ? LV_GRID_ALIGN_START : LV_GRID_ALIGN_END, col, 1, LV_GRID_ALIGN_CENTER,
                             row, 1);
    };
    place(label(_table, &lv_font_montserrat_12, kAccentDown, "\xE2\x80\xA2 DISH"), 1, 0);
    place(label(_table, &lv_font_montserrat_12, kOk, "\xE2\x80\xA2 TARGET"), 2, 0);
    for (int r = 0; r < kRows; ++r) {
        _rowLabel[r] = label(_table, &lv_font_montserrat_12, kMuted, "");
        _cur[r] = label(_table, &lv_font_montserrat_14, kText, fmt::kUnavailable);
        _want[r] = label(_table, &lv_font_montserrat_14, kText, fmt::kUnavailable);
        place(_rowLabel[r], 0, r + 1);
        place(_cur[r], 1, r + 1);
        place(_want[r], 2, r + 1);
    }

    _guide = label(_side, &lv_font_montserrat_14, kText, "");
    lv_obj_set_width(_guide, LV_PCT(100));
    lv_obj_set_style_text_align(_guide, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(_guide, LV_LABEL_LONG_WRAP);
}

void AlignmentScreen::applyLayout(bool landscape) {
    if (_layoutApplied && landscape == _landscape) return;
    _layoutApplied = true;
    _landscape = landscape;

    // "ACCURACY": the dish's own uncertainty about its attitude estimate.
    static const char* const kLong[] = {"AZIMUTH", "ELEVATION", "TILT", "ACCURACY"};
    static const char* const kShort[] = {"AZ", "EL", "TILT", "+/-"};
    for (int r = 0; r < kRows; ++r) lv_label_set_text(_rowLabel[r], landscape ? kShort[r] : kLong[r]);

    if (landscape) {
        // 320x240: plot left, table + guidance right.
        lv_obj_set_flex_flow(_body, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(_body, 6, 0);
        lv_obj_set_size(_plot.obj(), LV_PCT(48), LV_PCT(100));
        lv_obj_set_flex_grow(_plot.obj(), 0);
        lv_obj_set_size(_side, LV_SIZE_CONTENT, LV_PCT(100));
        lv_obj_set_flex_grow(_side, 1);
    } else {
        // 240x320: plot on top (takes the spare height), table below.
        lv_obj_set_flex_flow(_body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(_body, 6, 0);
        lv_obj_set_size(_plot.obj(), LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(_plot.obj(), 1);
        lv_obj_set_size(_side, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(_side, 0);
    }
}

void AlignmentScreen::update(const starlink::StarlinkSnapshot& snap) {
    if (!built()) return;
    applyLayout(lv_display_get_horizontal_resolution(nullptr) > lv_display_get_vertical_resolution(nullptr));

    // Without fresh data, show nothing rather than stale pointing.
    const bool fresh = snap.state == starlink::LinkState::Online && snap.hasStatus;
    const starlink::StarlinkStatus empty;
    const starlink::StarlinkStatus& s = fresh ? snap.status : empty;

    // Attitude estimate state, symbol + colour. (Uncertainty is in the table;
    // the built-in fonts have no "±" glyph, hence "+/-".)
    char buf[48];
    if (s.attitudeState) {
        const AttitudeState a = *s.attitudeState;
        const bool ok = a == AttitudeState::Converged;
        const bool busy = a == AttitudeState::Unconverged;
        snprintf(buf, sizeof(buf), "%s %s", ok ? LV_SYMBOL_OK : busy ? LV_SYMBOL_REFRESH : LV_SYMBOL_WARNING,
                 starlink::attitudeStateName(a));
        setText(_attitude, buf);
        setColor(_attitude, ok ? kOk : busy ? kWarn : kFail);
    } else {
        setText(_attitude, fresh ? "attitude --" : "no data");
        setColor(_attitude, kMuted);
    }

    // Plot
    std::optional<SkyPlot::Pointing> cur, want;
    if (s.azimuthDeg && s.elevationDeg) cur = SkyPlot::Pointing{*s.azimuthDeg, *s.elevationDeg};
    if (s.desiredAzimuthDeg && s.desiredElevationDeg) {
        want = SkyPlot::Pointing{*s.desiredAzimuthDeg, *s.desiredElevationDeg};
    }
    _plot.set(cur, want);

    // Table
    formatBearing(s.azimuthDeg, !_landscape, buf, sizeof(buf));
    setText(_cur[0], buf);
    formatBearing(s.desiredAzimuthDeg, !_landscape, buf, sizeof(buf));
    setText(_want[0], buf);
    formatDegrees(s.elevationDeg, buf, sizeof(buf));
    setText(_cur[1], buf);
    formatDegrees(s.desiredElevationDeg, buf, sizeof(buf));
    setText(_want[1], buf);
    formatDegrees(s.tiltDeg, buf, sizeof(buf));
    setText(_cur[2], buf);
    // Tilt from vertical that the target elevation implies.
    std::optional<float> wantTilt;
    if (s.desiredElevationDeg) wantTilt = 90.0f - *s.desiredElevationDeg;
    formatDegrees(wantTilt, buf, sizeof(buf));
    setText(_want[2], buf);
    if (s.attitudeUncertaintyDeg) {
        snprintf(buf, sizeof(buf), "+/-%.1f\xC2\xB0", *s.attitudeUncertaintyDeg);
    } else {
        snprintf(buf, sizeof(buf), "%s", fmt::kUnavailable);
    }
    setText(_cur[3], buf);
    setText(_want[3], "");

    // Guidance: plain differences, rounded as displayed.
    const align::Offset o = align::offset(s.azimuthDeg, s.elevationDeg, s.desiredAzimuthDeg, s.desiredElevationDeg);
    char turn[40] = "", raise[40] = "";
    if (o.azimuth) {
        const float d = roundf(*o.azimuth);
        if (d == 0) {
            snprintf(turn, sizeof(turn), "Bearing on target");
        } else {
            snprintf(turn, sizeof(turn), "%s Turn %.0f\xC2\xB0 %s", d > 0 ? LV_SYMBOL_RIGHT : LV_SYMBOL_LEFT,
                     fabsf(d), d > 0 ? "clockwise" : "counter-clockwise");
        }
    }
    if (o.elevation) {
        const float d = roundf(*o.elevation * 10) / 10;
        if (d == 0) {
            snprintf(raise, sizeof(raise), "Elevation on target");
        } else {
            snprintf(raise, sizeof(raise), "%s %s %.1f\xC2\xB0", d > 0 ? LV_SYMBOL_UP : LV_SYMBOL_DOWN,
                     d > 0 ? "Raise" : "Lower", fabsf(d));
        }
    }

    const char* warning = "";
    uint32_t color = kText;
    if (!fresh) {
        warning = "No data from dish";
        color = kMuted;
    } else if (s.attitudeState && *s.attitudeState != AttitudeState::Converged) {
        warning = LV_SYMBOL_WARNING " Attitude not converged:\nvalues are approximate";
        color = kWarn;
    } else if (!o.azimuth && !o.elevation) {
        warning = "Dish reports no pointing data";
        color = kMuted;
    }

    char guide[160];
    if (warning[0] && (turn[0] || raise[0])) {
        snprintf(guide, sizeof(guide), "%s\n%s%s%s", warning, turn, turn[0] && raise[0] ? "\n" : "", raise);
    } else if (warning[0]) {
        snprintf(guide, sizeof(guide), "%s", warning);
    } else {
        snprintf(guide, sizeof(guide), "%s%s%s", turn, turn[0] && raise[0] ? "\n" : "", raise);
    }
    setText(_guide, guide);
    setColor(_guide, color);
}

}  // namespace ui
