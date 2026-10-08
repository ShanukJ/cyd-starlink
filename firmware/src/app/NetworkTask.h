#pragma once

#include "../network/WifiManager.h"
#include "../starlink/StarlinkService.h"

namespace app {

// Starts the single network task (core 0) that drives WiFi, the setup
// portal and Starlink polling. Everything in it may block on I/O; nothing
// in it touches LVGL.
void startNetworkTask(net::WifiManager& wifi, starlink::StarlinkService& starlink);

}  // namespace app
