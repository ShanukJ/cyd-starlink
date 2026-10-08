#pragma once

#include <lvgl.h>

#include "../network/WifiStatus.h"

namespace ui {

// Shown while the setup access point is up: AP name, password, a WiFi QR
// code to join it, the setup URL and live connection progress.
class WifiSetupScreen {
public:
    using CloseFn = void (*)(void* ctx);

    void build(CloseFn onClose, void* ctx);
    void update(const net::WifiStatus& s);
    lv_obj_t* screen() const { return _screen; }

private:
    static void onCloseClicked(lv_event_t* e);

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _close = nullptr;
    lv_obj_t* _qr = nullptr;
    lv_obj_t* _apSsid = nullptr;
    lv_obj_t* _apPassword = nullptr;
    lv_obj_t* _url = nullptr;
    lv_obj_t* _state = nullptr;
    CloseFn _onClose = nullptr;
    void* _ctx = nullptr;
    char _qrText[80] = "";
    char _stateText[96] = "";
};

}  // namespace ui
