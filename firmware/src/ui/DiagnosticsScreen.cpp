#include "DiagnosticsScreen.h"

#include <IPAddress.h>
#include <stdio.h>
#include <string.h>

#include "../app/Version.h"
#include "Format.h"
#include "Theme.h"

namespace ui {

using namespace theme;
using starlink::CheckLevel;

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

struct LevelLook {
    const char* symbol;
    uint32_t color;
};

// Symbol + colour per level; the detail text carries the meaning too.
LevelLook lookFor(CheckLevel l) {
    switch (l) {
        case CheckLevel::Ok: return {LV_SYMBOL_OK, kOk};
        case CheckLevel::Info: return {LV_SYMBOL_REFRESH, kAccentDown};
        case CheckLevel::Warn: return {LV_SYMBOL_WARNING, kWarn};
        case CheckLevel::Fail: return {LV_SYMBOL_CLOSE, kFail};
        case CheckLevel::Unknown: break;
    }
    return {"?", kMuted};
}

}  // namespace

void DiagnosticsScreen::build(const char* boardName) {
    _boardName = boardName;

    _screen = lv_obj_create(nullptr);
    // Screens are built on demand and deleted when not shown (saves heap);
    // drop every pointer into the LVGL tree when that happens.
    lv_obj_add_event_cb(
        _screen, [](lv_event_t* e) { static_cast<DiagnosticsScreen*>(lv_event_get_user_data(e))->onDeleted(); },
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
    lv_obj_t* title = label(header, &lv_font_montserrat_14, kText, "DIAGNOSTICS");
    lv_obj_set_style_text_letter_space(title, 3, 0);
    _summary = label(header, &lv_font_montserrat_12, kMuted, "");

    _body = container(_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_width(_body, LV_PCT(100));
    lv_obj_set_flex_grow(_body, 1);

    _checks = container(_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_checks, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    for (Row& r : _rows) {
        lv_obj_t* row = container(_checks, LV_FLEX_FLOW_ROW);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 4, 0);
        r.mark = label(row, &lv_font_montserrat_14, kMuted, "?");
        lv_obj_set_width(r.mark, 14);
        lv_obj_set_style_text_align(r.mark, LV_TEXT_ALIGN_CENTER, 0);
        r.name = label(row, &lv_font_montserrat_12, kText, "");
        lv_obj_set_flex_grow(r.name, 1);
        lv_label_set_long_mode(r.name, LV_LABEL_LONG_CLIP);
        lv_obj_set_height(r.name, lv_font_get_line_height(&lv_font_montserrat_12));
        r.detail = label(row, &lv_font_montserrat_12, kMuted, fmt::kUnavailable);
        lv_obj_set_style_text_align(r.detail, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(r.detail, LV_LABEL_LONG_DOT);
        lv_obj_set_style_max_width(r.detail, LV_PCT(55), 0);
        lv_obj_set_height(r.detail, lv_font_get_line_height(&lv_font_montserrat_12));
        r.shownLevel = CheckLevel::Unknown;
        r.shown = false;
    }

    // Versions: DISH / MONITOR / WIFI sections, each a small title + value.
    // Values contain line breaks and otherwise wrap at "." or spaces.
    _info = container(_body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_info, 2, 0);
    lv_obj_set_style_border_width(_info, 1, 0);
    lv_obj_set_style_border_color(_info, lv_color_hex(kDivider), 0);
    static const char* const kTitles[kInfoSections] = {"DISH", "MONITOR", "WIFI"};
    for (int i = 0; i < kInfoSections; ++i) {
        _section[i] = container(_info, LV_FLEX_FLOW_ROW);
        lv_obj_set_size(_section[i], LV_PCT(100), LV_SIZE_CONTENT);
        _sectionTitle[i] = label(_section[i], &lv_font_montserrat_12, kMuted, kTitles[i]);
        _sectionValue[i] = label(_section[i], &lv_font_montserrat_12, kText, fmt::kUnavailable);
        lv_label_set_long_mode(_sectionValue[i], LV_LABEL_LONG_WRAP);
        if (i == 0) {
            _dishFirmware = label(_info, &lv_font_montserrat_12, kMuted, fmt::kUnavailable);
            lv_obj_set_width(_dishFirmware, LV_PCT(100));
            lv_label_set_long_mode(_dishFirmware, LV_LABEL_LONG_WRAP);
        }
    }
}

void DiagnosticsScreen::applyLayout(bool landscape) {
    if (_layoutApplied && landscape == _landscape) return;
    _layoutApplied = true;
    _landscape = landscape;

    if (landscape) {
        // 320x240: checks left, version sections stacked on the right.
        lv_obj_set_flex_flow(_body, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(_body, 8, 0);
        lv_obj_set_size(_checks, LV_PCT(60), LV_PCT(100));
        lv_obj_set_flex_grow(_checks, 0);
        lv_obj_set_size(_info, LV_SIZE_CONTENT, LV_PCT(100));
        lv_obj_set_flex_grow(_info, 1);
        lv_obj_set_flex_align(_info, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_border_side(_info, LV_BORDER_SIDE_LEFT, 0);
        lv_obj_set_style_pad_top(_info, 0, 0);
        lv_obj_set_style_pad_left(_info, 8, 0);
    } else {
        // 240x320: checks on top, version sections below (title beside value).
        lv_obj_set_flex_flow(_body, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_row(_body, 4, 0);
        lv_obj_set_size(_checks, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(_checks, 1);
        lv_obj_set_size(_info, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(_info, 0);
        lv_obj_set_flex_align(_info, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_border_side(_info, LV_BORDER_SIDE_TOP, 0);
        lv_obj_set_style_pad_top(_info, 4, 0);
        lv_obj_set_style_pad_left(_info, 0, 0);
    }
    for (int i = 0; i < kInfoSections; ++i) {
        lv_obj_set_flex_flow(_section[i], landscape ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(_section[i], 6, 0);
        lv_obj_set_width(_sectionTitle[i], landscape ? LV_SIZE_CONTENT : 62);
        lv_obj_set_width(_sectionValue[i], landscape ? LV_PCT(100) : LV_SIZE_CONTENT);
        lv_obj_set_flex_grow(_sectionValue[i], landscape ? 0 : 1);
    }
}

void DiagnosticsScreen::update(const starlink::StarlinkSnapshot& snap, const net::WifiStatus& wifi) {
    if (!built()) return;
    applyLayout(lv_display_get_horizontal_resolution(nullptr) > lv_display_get_vertical_resolution(nullptr));

    // Stale data is worse than none here: without a fresh poll every check
    // reads "--".
    const bool fresh = snap.state == starlink::LinkState::Online && snap.hasStatus;
    const starlink::StarlinkStatus empty;
    const starlink::StarlinkStatus& s = fresh ? snap.status : empty;

    starlink::Check checks[starlink::kCheckCount];
    starlink::evaluateChecks(s, checks);

    int problems = 0;
    for (int i = 0; i < starlink::kCheckCount; ++i) {
        Row& r = _rows[i];
        const starlink::Check& c = checks[i];
        setText(r.name, c.name);
        setText(r.detail, c.detail);
        if (!r.shown || c.level != r.shownLevel) {
            const LevelLook look = lookFor(c.level);
            lv_label_set_text(r.mark, look.symbol);
            lv_obj_set_style_text_color(r.mark, lv_color_hex(look.color), 0);
            // Problems also colour the detail text; fine states stay quiet.
            const bool loud = c.level == CheckLevel::Warn || c.level == CheckLevel::Fail;
            lv_obj_set_style_text_color(r.detail, lv_color_hex(loud ? look.color : kMuted), 0);
            r.shownLevel = c.level;
            r.shown = true;
        }
        if (c.level == CheckLevel::Warn || c.level == CheckLevel::Fail) problems++;
    }

    char buf[80];
    if (!fresh) {
        snprintf(buf, sizeof(buf), "no data");
    } else if (problems == 0) {
        snprintf(buf, sizeof(buf), LV_SYMBOL_OK " all clear");
    } else {
        snprintf(buf, sizeof(buf), LV_SYMBOL_WARNING " %d issue%s", problems, problems > 1 ? "s" : "");
    }
    setText(_summary, buf);
    setColor(_summary, !fresh ? kMuted : problems ? kWarn : kOk);

    // Versions: keep the last known dish versions even when stale; they
    // don't change while the dish is unreachable.
    const starlink::StarlinkStatus& last = snap.status;
    setText(_sectionValue[0], last.hardwareVersion[0] ? last.hardwareVersion : fmt::kUnavailable);
    setText(_dishFirmware, last.softwareVersion[0] ? last.softwareVersion : fmt::kUnavailable);
    // Landscape's narrow column would wrap the board name over 3 lines; it
    // is still shown in portrait and on the hardware test screen.
    snprintf(buf, sizeof(buf), "v" SM_VERSION "%s%s", _landscape ? "" : "\n", _landscape ? "" : _boardName);
    setText(_sectionValue[1], buf);
    if (wifi.sta == net::StaState::Connected) {
        snprintf(buf, sizeof(buf), "%s%s%d dBm", IPAddress(wifi.ip).toString().c_str(), _landscape ? "\n" : "  ",
                 wifi.rssi);
    } else {
        snprintf(buf, sizeof(buf), "Not connected");
    }
    setText(_sectionValue[2], buf);
}

}  // namespace ui
