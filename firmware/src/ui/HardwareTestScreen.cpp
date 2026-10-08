#include "HardwareTestScreen.h"

#include <Arduino.h>

#include "../app/Version.h"
#include "Theme.h"

namespace ui {

using namespace theme;

namespace {

constexpr uint8_t kCrosshairSize = 21;

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t color, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, text);
    return l;
}

lv_obj_t* makeRow(lv_obj_t* parent) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

void addColourBar(lv_obj_t* parent, uint32_t colour, uint32_t textColour, const char* name) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_height(box, 22);
    lv_obj_set_flex_grow(box, 1);
    lv_obj_set_style_bg_color(box, lv_color_hex(colour), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_t* l = makeLabel(box, &lv_font_montserrat_12, textColour, name);
    lv_obj_center(l);
}

}  // namespace

void HardwareTestScreen::build(Callback onBack, void* ctx) {
    _onBack = onBack;
    _ctx = ctx;
    const auto& status = _board.status();
    _screen = lv_obj_create(nullptr);
    lv_obj_t* scr = _screen;
    lv_obj_set_style_bg_color(scr, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 8, 0);
    lv_obj_set_style_pad_row(scr, 5, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

    // Header
    lv_obj_t* titleRow = makeRow(scr);
    makeLabel(titleRow, &lv_font_montserrat_20, kText, "STARLINK MONITOR");
    lv_obj_t* back = lv_button_create(titleRow);
    lv_obj_set_size(back, 40, 28);
    lv_obj_set_style_bg_color(back, lv_color_hex(kDivider), 0);
    lv_obj_center(makeLabel(back, &lv_font_montserrat_14, kText, LV_SYMBOL_LEFT));
    lv_obj_add_event_cb(back, onBackClicked, LV_EVENT_CLICKED, this);
    makeLabel(scr, &lv_font_montserrat_12, kMuted, "HARDWARE TEST  \xC2\xB7  v" SM_VERSION);

    lv_obj_t* divider = lv_obj_create(scr);
    lv_obj_remove_style_all(divider);
    lv_obj_set_size(divider, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(divider, lv_color_hex(kDivider), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

    // Checklist — every mark reflects a measured result.
    char buf[48];
    _displayValue = addRow(scr, "Display");
    refreshDisplayRow();

    // Controller row: verified by reading the power-mode register back.
    lv_obj_t* panel = addRow(scr, _board.display().controllerName());
    const hw::PanelReadback& rb = status.readback;
    if (status.displayAwake) {
        snprintf(buf, sizeof(buf), "ID %02X%02X%02X", rb.id[0], rb.id[1], rb.id[2]);
        setRow(panel, Mark::Ok, buf);
    } else if (status.displayReadable) {
        snprintf(buf, sizeof(buf), "PM %02X?", rb.powerMode);
        setRow(panel, Mark::Warn, buf);
    } else {
        setRow(panel, Mark::Warn, "no readback");
    }

    snprintf(buf, sizeof(buf), "v%d.%d.%d", lv_version_major(), lv_version_minor(), lv_version_patch());
    setRow(addRow(scr, "LVGL"), Mark::Ok, buf);

    _touchValue = addRow(scr, "Touch");
    hw::TouchDriver* touch = _board.touch();
    if (touch) {
        setRow(_touchValue, Mark::Pending, "tap screen");
    } else {
        setRow(_touchValue, Mark::Fail, "unavailable");
    }

    lv_obj_t* touchCtl = addRow(scr, touch ? touch->controllerName() : "Touch IC");
    setRow(touchCtl, status.touchOk ? Mark::Ok : Mark::Fail, status.touchOk ? "ready" : "init failed");

    _wifiValue = addRow(scr, "WiFi");
    setRow(_wifiValue, Mark::NotImplemented, "--");
    _starlinkValue = addRow(scr, "Starlink");
    setRow(_starlinkValue, Mark::NotImplemented, "--");

    // Live touch readout
    _coords = makeLabel(scr, &lv_font_montserrat_20, kText, "X: ---  Y: ---");
    _raw = makeLabel(scr, &lv_font_montserrat_12, kMuted, "");

    // Colour bars: if RED shows blue, flip `bgr`; if black is white, flip `invert`.
    lv_obj_t* bars = makeRow(scr);
    lv_obj_set_style_pad_column(bars, 2, 0);
    addColourBar(bars, 0xFF0000, 0xFFFFFF, "RED");
    addColourBar(bars, 0x00FF00, 0x000000, "GREEN");
    addColourBar(bars, 0x0000FF, 0xFFFFFF, "BLUE");
    addColourBar(bars, 0xFFFFFF, 0x000000, "WHITE");

    // Controls
    lv_obj_t* controls = makeRow(scr);
    lv_obj_set_style_pad_column(controls, 12, 0);
    lv_obj_t* rotate = lv_button_create(controls);
    lv_obj_set_height(rotate, 32);
    lv_obj_add_event_cb(rotate, onRotate, LV_EVENT_CLICKED, this);
    lv_obj_center(makeLabel(rotate, &lv_font_montserrat_14, kText, LV_SYMBOL_REFRESH " Rotate"));

    lv_obj_t* slider = lv_slider_create(controls);
    lv_obj_set_flex_grow(slider, 1);
    lv_obj_set_style_margin_right(slider, 8, 0);
    lv_slider_set_range(slider, 10, 255);
    lv_slider_set_value(slider, _board.profile().backlight.defaultLevel, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, onBrightness, LV_EVENT_VALUE_CHANGED, this);
    if (!_board.status().backlightDimmable) lv_obj_add_state(slider, LV_STATE_DISABLED);

    // Crosshair follows the finger on the top layer, above everything.
    _crosshair = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(_crosshair);
    lv_obj_set_size(_crosshair, kCrosshairSize, kCrosshairSize);
    lv_obj_set_style_radius(_crosshair, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(_crosshair, 2, 0);
    lv_obj_set_style_border_color(_crosshair, lv_color_hex(0xFFD33D), 0);
    lv_obj_remove_flag(_crosshair, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_crosshair, LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(onTimer, 50, this);
}

lv_obj_t* HardwareTestScreen::addRow(lv_obj_t* parent, const char* name) {
    lv_obj_t* row = makeRow(parent);
    makeLabel(row, &lv_font_montserrat_14, kText, name);
    return makeLabel(row, &lv_font_montserrat_14, kMuted, "");
}

void HardwareTestScreen::setRow(lv_obj_t* value, Mark mark, const char* text) {
    // Symbol + text + colour: state is readable without relying on colour.
    const char* symbol = "";
    uint32_t colour = kMuted;
    switch (mark) {
        case Mark::Ok: symbol = LV_SYMBOL_OK " "; colour = kOk; break;
        case Mark::Warn: symbol = LV_SYMBOL_WARNING " "; colour = kWarn; break;
        case Mark::Fail: symbol = LV_SYMBOL_CLOSE " "; colour = kFail; break;
        case Mark::Pending: symbol = "? "; colour = kWarn; break;
        case Mark::Busy: symbol = LV_SYMBOL_REFRESH " "; colour = kWarn; break;
        case Mark::NotImplemented: break;
    }
    lv_label_set_text_fmt(value, "%s%s", symbol, text);
    lv_obj_set_style_text_color(value, lv_color_hex(colour), 0);
}

void HardwareTestScreen::refreshDisplayRow() {
    auto& d = _board.display();
    char buf[32];
    snprintf(buf, sizeof(buf), "%ux%u  r%u", d.width(), d.height(), d.rotation());
    setRow(_displayValue, _board.status().displayOk ? Mark::Ok : Mark::Fail, buf);
}

void HardwareTestScreen::setWifiStatus(const net::WifiStatus& s) {
    Mark mark = Mark::Warn;
    char text[24];
    switch (s.sta) {
        case net::StaState::NotConfigured:
            strlcpy(text, "not set up", sizeof(text));
            break;
        case net::StaState::Connecting:
            mark = Mark::Busy;
            strlcpy(text, "connecting", sizeof(text));
            break;
        case net::StaState::WaitingRetry:
            if (s.failures) {
                snprintf(text, sizeof(text), "retry %lus", (unsigned long)((s.retryInMs + 999) / 1000));
            } else {
                mark = Mark::Busy;
                strlcpy(text, "reconnecting", sizeof(text));
            }
            break;
        case net::StaState::Connected:
            mark = Mark::Ok;
            strlcpy(text, IPAddress(s.ip).toString().c_str(), sizeof(text));
            break;
    }
    if (mark != _shownWifiMark || strcmp(text, _shownWifiText) != 0) {
        _shownWifiMark = mark;
        strlcpy(_shownWifiText, text, sizeof(_shownWifiText));
        setRow(_wifiValue, mark, text);
    }
}

void HardwareTestScreen::setStarlink(const starlink::StarlinkSnapshot& s) {
    using starlink::HealthState;
    Mark mark = Mark::NotImplemented;
    char text[24];
    const char* reason = s.health.reason;
    switch (s.health.state) {
        case HealthState::Connecting:
            mark = s.state == starlink::LinkState::Waiting ? Mark::NotImplemented : Mark::Busy;
            strlcpy(text, s.state == starlink::LinkState::Waiting ? "--" : "connecting", sizeof(text));
            break;
        case HealthState::Online:
            mark = Mark::Ok;
            snprintf(text, sizeof(text), "online %lums", (unsigned long)s.lastRttMs);
            break;
        case HealthState::Degraded:
            mark = Mark::Warn;
            strlcpy(text, reason, sizeof(text));
            break;
        case HealthState::Offline:
        case HealthState::Error:
            mark = Mark::Fail;
            strlcpy(text, reason, sizeof(text));
            break;
    }
    if (mark != _shownStarlinkMark || strcmp(text, _shownStarlinkText) != 0) {
        _shownStarlinkMark = mark;
        strlcpy(_shownStarlinkText, text, sizeof(_shownStarlinkText));
        setRow(_starlinkValue, mark, text);
    }
}

void HardwareTestScreen::update() {
    const TouchState& t = _port.touch();

    // The crosshair lives on the top layer; keep it off other screens.
    if (lv_screen_active() != _screen) {
        lv_obj_add_flag(_crosshair, LV_OBJ_FLAG_HIDDEN);
        _shownPressed = false;
        return;
    }

    if (t.pressCount != _shownPressCount && t.pressCount > 0) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%lu presses", (unsigned long)t.pressCount);
        setRow(_touchValue, Mark::Ok, buf);
        _shownPressCount = t.pressCount;
    }

    if (t.pressed && (t.point.x != _shownPoint.x || t.point.y != _shownPoint.y || !_shownPressed)) {
        lv_label_set_text_fmt(_coords, "X: %3d  Y: %3d", t.point.x, t.point.y);
        lv_obj_set_pos(_crosshair, t.point.x - kCrosshairSize / 2, t.point.y - kCrosshairSize / 2);
        lv_obj_remove_flag(_crosshair, LV_OBJ_FLAG_HIDDEN);
        _shownPoint = t.point;
    } else if (!t.pressed && _shownPressed) {
        lv_obj_add_flag(_crosshair, LV_OBJ_FLAG_HIDDEN);
    }
    _shownPressed = t.pressed;

    // PENIRQ level is diagnostic only (the driver polls); it should read
    // LOW while pressed if the IRQ line is wired.
    const int8_t irqPin = _board.profile().touch.irq;
    const int irq = irqPin >= 0 ? digitalRead(irqPin) : -1;

    if (t.raw.x != _shownRaw.x || t.raw.y != _shownRaw.y || irq != _shownIrq) {
        _shownRaw = t.raw;
        _shownIrq = irq;
        lv_label_set_text_fmt(_raw, "raw %4u,%4u  z %-3u  IRQ %s", t.raw.x, t.raw.y, t.raw.pressure,
                              irq < 0 ? "n/a" : (irq ? "high" : "LOW"));
    }
}

void HardwareTestScreen::onTimer(lv_timer_t* timer) {
    static_cast<HardwareTestScreen*>(lv_timer_get_user_data(timer))->update();
}

void HardwareTestScreen::onRotate(lv_event_t* e) {
    auto* self = static_cast<HardwareTestScreen*>(lv_event_get_user_data(e));
    self->_port.setRotation((self->_port.rotation() + 1) & 3);
    self->refreshDisplayRow();
}

void HardwareTestScreen::onBackClicked(lv_event_t* e) {
    auto* self = static_cast<HardwareTestScreen*>(lv_event_get_user_data(e));
    if (self->_onBack) self->_onBack(self->_ctx);
}

void HardwareTestScreen::onBrightness(lv_event_t* e) {
    auto* self = static_cast<HardwareTestScreen*>(lv_event_get_user_data(e));
    auto* slider = static_cast<lv_obj_t*>(lv_event_get_target(e));
    self->_board.backlight().set(static_cast<uint8_t>(lv_slider_get_value(slider)));
}

}  // namespace ui
