#include "NetworkTask.h"

#include <Arduino.h>

namespace app {

namespace {

constexpr uint32_t kStackBytes = 8192;

struct Context {
    config::SettingsStore* settings;
    net::WifiManager* wifi;
    net::WebUi* web;
    starlink::StarlinkService* starlink;
};
Context s_ctx;

void run(void*) {
    config::Settings settings = s_ctx.settings->get();
    uint32_t settingsVersion = s_ctx.settings->version();
    for (;;) {
        if (s_ctx.settings->version() != settingsVersion) {  // picked up within one loop
            settingsVersion = s_ctx.settings->version();
            settings = s_ctx.settings->get();
        }
        net::WifiManager& wifi = *s_ctx.wifi;
        wifi.loop();
        s_ctx.web->loop();
        // Don't poll the dish during setup: the portal shares this task and a
        // slow dish request would make the setup page sluggish.
        const bool networkUp = wifi.online() && !wifi.portalActive();
        s_ctx.starlink->loop(millis(), networkUp, settings.starlinkHost, settings.pollMs);
        // Short sleep while the portal is up so DNS/HTTP stay responsive.
        vTaskDelay(pdMS_TO_TICKS(wifi.portalActive() ? 5 : 10));
    }
}

}  // namespace

void startNetworkTask(config::SettingsStore& settings, net::WifiManager& wifi, net::WebUi& web,
                      starlink::StarlinkService& starlink) {
    s_ctx = {&settings, &wifi, &web, &starlink};
    // Core 0 alongside the WiFi stack; the UI loop runs on core 1.
    xTaskCreatePinnedToCore(run, "net", kStackBytes, nullptr, 1, nullptr, 0);
}

}  // namespace app
