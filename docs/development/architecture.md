# Architecture

```
            ┌───────────────────────────────┐
  UI        │ screens (LVGL)                │  knows nothing about pins or controllers
            ├───────────────────────────────┤
  Port      │ LvglPort                      │  flush (DMA, double buffer), input, tick
            ├───────────────────────────────┤
  Hardware  │ Board                         │  builds drivers from a HardwareProfile
            │  DisplayDriver  TouchDriver   │  abstract interfaces
            │  TouchMapper    Backlight     │  calibration + rotation in ONE place
            ├───────────────────────────────┤
  Drivers   │ LgfxSpiDisplay  Xpt2046Touch  │  only files that include LovyanGFX
            ├───────────────────────────────┤
  Profiles  │ esp32_2432s028_st7789.h ...   │  pure data: pins, quirks, calibration
            └───────────────────────────────┘
```

Starlink support is a separate layer beside the UI:

```
  StarlinkService   scheduling, backoff, health, thread-safe snapshot (network task)
  StarlinkClient    sends requests, maps results
  StatusDecoder     Response protobuf -> StarlinkStatus (pure C++, host-tested)
  Health            StarlinkSnapshot -> ONLINE/DEGRADED/... (pure C++, host-tested)
  ProtoReader       bounds-checked protobuf wire reader (no codegen)
  StarlinkTransport interface; GrpcWebTransport = HTTP/1.1 to port 9201
```

The UI consumes `StarlinkSnapshot` / `StarlinkStatus` only and never sees
the transport. Protocol details: [starlink-grpc-web.md](../protocol/starlink-grpc-web.md).
Data model, rules for missing values, and health rules: [data-model.md](data-model.md).

## Adding a board

1. Add `firmware/src/hardware/profiles/<board>.h` with a `HardwareProfile`.
2. Add a `HW_PROFILE_<BOARD>` branch to `profiles/Profiles.h`.
3. Add an `[env:<board>]` to `platformio.ini` with that flag.
4. If the board has a new controller, add an enum value and a driver
   (a new LovyanGFX panel type is usually a one-line `case` in
   `LgfxSpiDisplay::begin`).

No UI or Starlink code should change.

## Rotation and touch

Rotation follows the LovyanGFX convention documented in `DisplayDriver.h`.
Touch calibration maps raw controller values to **native** panel pixels
(rotation 0); `TouchMapper` then applies the rotation. Calibration is
therefore independent of the orientation the UI chooses.

## Library choices

**Display: LovyanGFX** (used only as a panel driver; LVGL does all drawing)

- Has an explicit, maintained configuration for *this* board (ST7789 and
  ILI9341 CYDs, told apart by panel ID).
- Runtime, object-based configuration: one source tree can build many boards
  from profile data, with no per-board `User_Setup.h` copies.
- Async DMA pushes fit LVGL's double-buffered flush.
- Ships drivers for XPT2046, GT911, CST816S and FT5x06 touch controllers,
  which covers the future CYD variants.

Rejected:

- **TFT_eSPI**: configured at compile time through global macros, which
  makes multi-board builds awkward. Its maintenance lags Arduino-ESP32 3.x.
- **Arduino_GFX**: a good panel library, but it has no touch drivers.
- **LVGL's built-in `lv_st7789`**: would need a hand-written SPI/DMA layer
  per board.

**Touch: LovyanGFX `Touch_XPT2046`, used standalone.** It provides the SPI
protocol and a 7-sample median filter. Calibration lives in `TouchMapper`, not
in the library.

**UI: LVGL 9.5** (pinned; 9.6 was three weeks old when this was written).

## Threading

| Task | Core | Runs |
|---|---|---|
| Arduino `loop()` | 1 | LVGL, touch, BOOT button, `UiManager` |
| `net` | 0 | `WifiManager` (station, retries, setup portal), `StarlinkService` (polls every 2 s) |

The UI never calls WiFi or network APIs. It reads `WifiStatus` / `StarlinkSnapshot` snapshots
(copied under a mutex) every 250 ms and sends requests as atomic flags
(`requestPortal()`, `requestPortalClose()`). The portal's `WebServer` can
block for seconds on a slow client, which is why it does not share the UI
task.

## Settings

`config::Settings` lives in NVS namespace `sm-config` with a `schema` key.
Bump the schema version when the layout changes, and migrate in
`config::load()`. After boot the network task owns the settings.

## Flash budget

The app partition is 1.9 MB (`min_spiffs.csv`, two OTA slots). Milestone 2
uses about 1.5 MB. The WiFi/TCP/WPA stack accounts for roughly 530 KB and
LVGL for about 300 KB. If space gets tight, disable unused LVGL widgets in
`lv_conf.h` first.
