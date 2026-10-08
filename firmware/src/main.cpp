// Starlink Monitor
//
// Task layout:
//   loop() (core 1)  UI: LVGL, touch, BOOT button
//   "net"  (core 0)  WifiManager (station, setup portal), StarlinkService (polling)
// The UI only reads snapshots from the network task; it never blocks on I/O.

#include <Arduino.h>

#include "app/NetworkTask.h"
#include "app/SerialCommands.h"
#include "app/Version.h"
#include "config/Settings.h"
#include "hardware/Board.h"
#include "hardware/Button.h"
#include "hardware/profiles/Profiles.h"
#include "network/WifiManager.h"
#include "starlink/StarlinkService.h"
#include "ui/LvglPort.h"
#include "ui/UiManager.h"
#include "utils/Log.h"

namespace {

constexpr uint32_t kSetupHoldMs = 3000;

hw::Board board(hw::kActiveProfile);
hw::Button setupButton(hw::kActiveProfile.button, kSetupHoldMs);
net::WifiManager wifi;
starlink::StarlinkService starlinkService;
ui::LvglPort lvglPort;
ui::UiManager uiManager(board, lvglPort, wifi, starlinkService);
app::SerialCommands serialCommands(board, uiManager, lvglPort);
bool uiRunning = false;

}  // namespace

void setup() {
    Serial.begin(115200);

    LOG("BOOT", "%s v%s (built %s)", SM_PROJECT_NAME, SM_VERSION, SM_BUILD_DATE);
    LOG("HW", "%s [%s]", board.profile().name, board.profile().id);
    LOG("HW", "%s rev %u, %lu MHz, flash %lu KB, free heap %lu B", ESP.getChipModel(), ESP.getChipRevision(),
        (unsigned long)getCpuFrequencyMhz(), (unsigned long)(ESP.getFlashChipSize() / 1024),
        (unsigned long)ESP.getFreeHeap());

    config::Settings settings;
    config::load(settings);

    const hw::BoardStatus& status = board.begin();
    setupButton.begin();

    if (status.displayOk && lvglPort.begin(board)) {
        uiManager.begin();
        // Render the first frame while the backlight is still off, then turn it on.
        lv_refr_now(nullptr);
        board.display().waitIdle();
        board.backlight().set(board.profile().backlight.defaultLevel);
        uiRunning = true;
    } else {
        LOG("BOOT", "Display unavailable - running headless");
    }

    // WiFi works without a display: the setup AP credentials are also logged.
    wifi.begin(settings);
    app::startNetworkTask(wifi, starlinkService);

    LOG("BOOT", "Ready. Free heap %lu B (min %lu B)", (unsigned long)ESP.getFreeHeap(),
        (unsigned long)ESP.getMinFreeHeap());
}

void loop() {
    if (setupButton.pollHeld(millis())) {
        LOG("INPUT", "BOOT button held - opening WiFi setup");
        wifi.requestPortal();
    }
    if (!uiRunning) {
        delay(20);
        return;
    }
    serialCommands.poll();
    const uint32_t idleMs = lvglPort.handle();
    delay(idleMs < 1 ? 1 : (idleMs > 10 ? 10 : idleMs));
}
