#pragma once

#include <lvgl.h>

#include "../hardware/Board.h"
#include "../network/WifiManager.h"
#include "../starlink/StarlinkService.h"
#include "AlignmentScreen.h"
#include "DashboardScreen.h"
#include "DiagnosticsScreen.h"
#include "HardwareTestScreen.h"
#include "LvglPort.h"
#include "WifiSetupScreen.h"

namespace ui {

// Owns the screens and decides which one is shown, based on app state
// polled from the other subsystems. Runs on the UI task only.
//
//   Pages (swipe left/right, or tap the page dots):
//     Dashboard <-> Alignment <-> Diagnostics
//   Dashboard --long-press--> Hardware test --back--> Dashboard
//   WiFi setup overrides everything while the setup portal is open.
class UiManager {
public:
    UiManager(hw::Board& board, LvglPort& port, net::WifiManager& wifi, starlink::StarlinkService& starlink)
        : _board(board), _wifi(wifi), _starlink(starlink), _test(board, port) {}

    void begin();
    // Shows page `index` (0 = dashboard). For serial dev commands.
    void showPage(int index) { goToPage(index, LV_SCREEN_LOAD_ANIM_NONE); }

private:
    static constexpr int kPageCount = 3;

    static void onTimer(lv_timer_t* timer);
    static void onSetupClose(void* ctx);
    static void onOpenHardwareTest(void* ctx);
    static void onCloseHardwareTest(void* ctx);
    static void onGesture(lv_event_t* e);
    static void onDotsClicked(lv_event_t* e);

    void update();
    lv_obj_t* pageScreen(int page) const;
    void goToPage(int page, lv_screen_load_anim_t anim);
    void buildPageDots();
    void showPageDots(bool visible);

    hw::Board& _board;
    net::WifiManager& _wifi;
    starlink::StarlinkService& _starlink;

    int _page = 0;                // index into the page list
    bool _hardwareTest = false;   // service screen open
    DashboardScreen _dashboard;
    AlignmentScreen _alignment;
    DiagnosticsScreen _diagnostics;
    HardwareTestScreen _test;
    WifiSetupScreen _setup;

    lv_obj_t* _dots = nullptr;
    lv_obj_t* _dot[kPageCount] = {};
};

}  // namespace ui
