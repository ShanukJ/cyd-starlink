#pragma once

#include <lvgl.h>

#include "../network/WifiStatus.h"
#include "../starlink/Diagnostics.h"
#include "../starlink/StarlinkSnapshot.h"

namespace ui {

// Per-subsystem checks (hardware, RF, GPS, network, thermal, obstruction,
// Ethernet, other alerts, software update) plus dish and monitor versions.
class DiagnosticsScreen {
public:
    // boardName: hardware profile name shown in the monitor info.
    void build(const char* boardName);
    void update(const starlink::StarlinkSnapshot& s, const net::WifiStatus& wifi);
    lv_obj_t* screen() const { return _screen; }
    bool built() const { return _screen != nullptr; }

private:
    struct Row {
        lv_obj_t* mark;
        lv_obj_t* name;
        lv_obj_t* detail;
        starlink::CheckLevel shownLevel;
        bool shown;
    };

    void applyLayout(bool landscape);
    void onDeleted() {
        _screen = nullptr;
        _layoutApplied = false;
    }

    lv_obj_t* _screen = nullptr;
    lv_obj_t* _summary = nullptr;
    lv_obj_t* _body = nullptr;
    lv_obj_t* _checks = nullptr;
    lv_obj_t* _info = nullptr;
    static constexpr int kInfoSections = 3;  // dish, monitor, WiFi
    lv_obj_t* _section[kInfoSections] = {};
    lv_obj_t* _sectionTitle[kInfoSections] = {};
    lv_obj_t* _sectionValue[kInfoSections] = {};
    lv_obj_t* _dishFirmware = nullptr;  // full width: too long for the value column
    Row _rows[starlink::kCheckCount] = {};
    const char* _boardName = "";

    bool _landscape = false;
    bool _layoutApplied = false;
};

}  // namespace ui
