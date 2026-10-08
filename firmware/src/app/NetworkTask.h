#pragma once

#include "../config/Settings.h"
#include "../network/WebUi.h"
#include "../network/WifiManager.h"
#include "../starlink/StarlinkService.h"

namespace app {

// Starts the single network task (core 0) that drives WiFi, the web UI
// (setup portal and LAN settings page) and Starlink polling. Everything in
// it may block on I/O; nothing in it touches LVGL.
void startNetworkTask(config::SettingsStore& settings, net::WifiManager& wifi, net::WebUi& web,
                      starlink::StarlinkService& starlink);

}  // namespace app
