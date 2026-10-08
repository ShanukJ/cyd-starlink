#include "NetworkTask.h"

#include <Arduino.h>

namespace app {

namespace {

constexpr uint32_t kStackBytes = 8192;

struct Context {
    net::WifiManager* wifi;
    starlink::StarlinkService* starlink;
};
Context s_ctx;

void run(void*) {
    for (;;) {
        net::WifiManager& wifi = *s_ctx.wifi;
        wifi.loop();
        // Don't poll the dish during setup: the portal shares this task and a
        // slow dish request would make the setup page sluggish.
        const bool networkUp = wifi.online() && !wifi.portalActive();
        s_ctx.starlink->loop(millis(), networkUp, wifi.settings().starlinkHost);
        // Short sleep while the portal is up so DNS/HTTP stay responsive.
        vTaskDelay(pdMS_TO_TICKS(wifi.portalActive() ? 5 : 20));
    }
}

}  // namespace

void startNetworkTask(net::WifiManager& wifi, starlink::StarlinkService& starlink) {
    s_ctx = {&wifi, &starlink};
    // Core 0 alongside the WiFi stack; the UI loop runs on core 1.
    xTaskCreatePinnedToCore(run, "net", kStackBytes, nullptr, 1, nullptr, 0);
}

}  // namespace app
