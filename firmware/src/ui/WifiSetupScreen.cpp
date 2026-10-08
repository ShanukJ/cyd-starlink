#include "WifiSetupScreen.h"

#include <stdio.h>
#include <string.h>

#include "Theme.h"

namespace ui {

namespace {

constexpr int32_t kQrSize = 112;

lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, const char* text) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, text);
    return l;
}

}  // namespace

void WifiSetupScreen::build(CloseFn onClose, void* ctx) {
    _onClose = onClose;
    _ctx = ctx;

    _screen = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(_screen, lv_color_hex(theme::kBg), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_screen, 10, 0);
    lv_obj_set_style_pad_row(_screen, 4, 0);
    lv_obj_set_flex_flow(_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Title row with a close button (only when a network is already saved).
    lv_obj_t* titleRow = lv_obj_create(_screen);
    lv_obj_remove_style_all(titleRow);
    lv_obj_set_size(titleRow, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(titleRow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(titleRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    label(titleRow, &lv_font_montserrat_20, theme::kText, LV_SYMBOL_WIFI " WIFI SETUP");
    _close = lv_button_create(titleRow);
    lv_obj_set_size(_close, 36, 30);
    lv_obj_set_style_bg_color(_close, lv_color_hex(theme::kDivider), 0);
    lv_obj_center(label(_close, &lv_font_montserrat_14, theme::kText, LV_SYMBOL_CLOSE));
    lv_obj_add_event_cb(_close, onCloseClicked, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(_close, LV_OBJ_FLAG_HIDDEN);

    // QR code: scanning it joins the setup network (standard WIFI: URI).
    _qr = lv_qrcode_create(_screen);
    lv_qrcode_set_size(_qr, kQrSize);
    lv_qrcode_set_dark_color(_qr, lv_color_black());
    lv_qrcode_set_light_color(_qr, lv_color_white());
    lv_obj_set_style_border_color(_qr, lv_color_white(), 0);
    lv_obj_set_style_border_width(_qr, 5, 0);  // quiet zone
    lv_obj_set_style_margin_top(_qr, 4, 0);

    label(_screen, &lv_font_montserrat_12, theme::kMuted, "Scan, or join this WiFi network:");
    _apSsid = label(_screen, &lv_font_montserrat_14, theme::kText, "");
    label(_screen, &lv_font_montserrat_12, theme::kMuted, "Password");
    _apPassword = label(_screen, &lv_font_montserrat_20, theme::kText, "");
    lv_obj_set_style_text_letter_space(_apPassword, 2, 0);
    _url = label(_screen, &lv_font_montserrat_12, theme::kMuted, "");

    _state = label(_screen, &lv_font_montserrat_14, theme::kWarn, "");
    lv_obj_set_width(_state, LV_PCT(100));
    lv_obj_set_style_text_align(_state, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(_state, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_margin_top(_state, 4, 0);
}

void WifiSetupScreen::update(const net::WifiStatus& s) {
    char qr[sizeof(_qrText)];
    snprintf(qr, sizeof(qr), "WIFI:T:WPA;S:%s;P:%s;;", s.apSsid, s.apPassword);
    if (strcmp(qr, _qrText) != 0) {
        strlcpy(_qrText, qr, sizeof(_qrText));
        lv_qrcode_update(_qr, _qrText, strlen(_qrText));
        lv_label_set_text(_apSsid, s.apSsid);
        lv_label_set_text(_apPassword, s.apPassword);
        lv_label_set_text_fmt(_url, "then open  http://%s", s.apIp);
    }

    if (s.ssid[0]) {
        lv_obj_remove_flag(_close, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_close, LV_OBJ_FLAG_HIDDEN);
    }

    // Progress line: symbol + text, so it doesn't rely on colour.
    char text[sizeof(_stateText)];
    uint32_t color = theme::kWarn;
    switch (s.sta) {
        case net::StaState::NotConfigured:
            strlcpy(text, s.portalClients ? "Phone connected - open the setup page" : "Waiting for setup...",
                    sizeof(text));
            color = theme::kMuted;
            break;
        case net::StaState::Connecting:
            snprintf(text, sizeof(text), LV_SYMBOL_REFRESH " Connecting to %s...", s.ssid);
            break;
        case net::StaState::WaitingRetry:
            if (s.failures) {
                snprintf(text, sizeof(text), LV_SYMBOL_WARNING " Can't join %s (%s)", s.ssid, s.lastError);
            } else {
                snprintf(text, sizeof(text), LV_SYMBOL_REFRESH " Connecting to %s...", s.ssid);
            }
            break;
        case net::StaState::Connected:
            snprintf(text, sizeof(text), LV_SYMBOL_OK " Online: %s", s.ssid);
            color = theme::kOk;
            break;
    }
    if (strcmp(text, _stateText) != 0) {
        strlcpy(_stateText, text, sizeof(_stateText));
        lv_label_set_text(_state, _stateText);
        lv_obj_set_style_text_color(_state, lv_color_hex(color), 0);
    }
}

void WifiSetupScreen::onCloseClicked(lv_event_t* e) {
    auto* self = static_cast<WifiSetupScreen*>(lv_event_get_user_data(e));
    if (self->_onClose) self->_onClose(self->_ctx);
}

}  // namespace ui
