// Starlink Monitor
//
// Task layout:
//   loop() (core 1)  UI: LVGL, touch, BOOT button
//   "net"  (core 0)  WifiManager (station, setup AP), WebUi (portal + LAN
//                    settings page), StarlinkService (polling)
// The UI only reads snapshots from the network task; it never blocks on I/O.
// Settings live in one SettingsStore that both tasks read.

#include <Arduino.h>

#include "app/NetworkTask.h"
#include "app/SerialCommands.h"
#include "app/Version.h"
#include "config/Settings.h"
#include "hardware/Board.h"
#include "hardware/Button.h"
#include "hardware/profiles/Profiles.h"
#include "network/WebUi.h"
#include "network/WifiManager.h"
#include "starlink/StarlinkService.h"
#include "ui/LvglPort.h"
#include "ui/UiManager.h"
#include "utils/Log.h"

namespace {

constexpr uint32_t kSetupHoldMs = 3000;

hw::Board board(hw::kActiveProfile);
hw::Button setupButton(hw::kActiveProfile.button, kSetupHoldMs);
config::SettingsStore settingsStore;
net::WifiManager wifi;
net::WebUi webUi;
starlink::StarlinkService starlinkService;
ui::LvglPort lvglPort;
ui::UiManager uiManager(board, lvglPort, wifi, starlinkService);
app::SerialCommands serialCommands(board, uiManager, lvglPort);
bool uiRunning = false;

// Brightness and orientation from the settings, applied when *they* change
// (so the hardware test screen's unsaved tweaks aren't undone by edits to
// unrelated settings).
struct DisplaySettings {
    uint32_t version = 0;
    int brightness = -1;
    int rotation = -1;
} appliedDisplay;

uint8_t effectiveRotation(const config::Settings& s) {
    return s.rotation == config::kRotationBoardDefault ? board.profile().defaultRotation : s.rotation;
}

void applyDisplaySettings(bool force) {
    if (!force && settingsStore.version() == appliedDisplay.version) return;
    appliedDisplay.version = settingsStore.version();
    const config::Settings s = settingsStore.get();
    if (force || s.brightness != appliedDisplay.brightness) {
        appliedDisplay.brightness = s.brightness;
        board.backlight().set(s.brightness);
    }
    if (force || s.rotation != appliedDisplay.rotation) {
        appliedDisplay.rotation = s.rotation;
        if (lvglPort.rotation() != effectiveRotation(s)) lvglPort.setRotation(effectiveRotation(s));
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);

    LOG("BOOT", "%s v%s (commit %s, built %s)", SM_PROJECT_NAME, SM_VERSION, SM_GIT_COMMIT, SM_BUILD_DATE);
    LOG("HW", "%s [%s]", board.profile().name, board.profile().id);
    LOG("HW", "%s rev %u, %lu MHz, flash %lu KB, free heap %lu B", ESP.getChipModel(), ESP.getChipRevision(),
        (unsigned long)getCpuFrequencyMhz(), (unsigned long)(ESP.getFlashChipSize() / 1024),
        (unsigned long)ESP.getFreeHeap());

    settingsStore.begin();

    const hw::BoardStatus& status = board.begin();
    setupButton.begin();

    if (status.displayOk && lvglPort.begin(board)) {
        const config::Settings s = settingsStore.get();
        if (lvglPort.rotation() != effectiveRotation(s)) lvglPort.setRotation(effectiveRotation(s));
        uiManager.begin();
        // Render the first frame while the backlight is still off, then turn it on.
        lv_refr_now(nullptr);
        board.display().waitIdle();
        applyDisplaySettings(true);
        uiRunning = true;
    } else {
        LOG("BOOT", "Display unavailable - running headless");
    }

    // WiFi works without a display: the setup AP credentials are also logged.
    wifi.begin(settingsStore);
    webUi.begin({&settingsStore, &wifi, &starlinkService, board.profile().name, board.profile().defaultRotation});
    app::startNetworkTask(settingsStore, wifi, webUi, starlinkService);

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
    applyDisplaySettings(false);
    const uint32_t idleMs = lvglPort.handle();
    delay(idleMs < 1 ? 1 : (idleMs > 10 ? 10 : idleMs));
}
