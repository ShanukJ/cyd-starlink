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

## Memory

There is no PSRAM, so RAM (about 230 KB of heap at boot) is the tightest
resource. Measured at Milestone 8:

| Consumer | Heap |
|---|---|
| LVGL core + 2 x 19.2 KB DMA draw buffers | ~47 KB |
| One screen's LVGL objects | 5 - 17 KB |
| WiFi stack, network task | ~60 KB |
| Free after boot | ~107 KB (largest block ~63 KB) |

Because of this, **screens are built when shown and deleted when left**
(`UiManager`). Building all six screens at boot cost 62 KB and left the
largest free block at 16 KB. Each screen class keeps its non-LVGL state
(history copy, DEGRADED hold) across rebuilds. It drops its pointers into
the LVGL tree from an `LV_EVENT_DELETE` handler. Navigation triggered from
inside an LVGL event is deferred with `lv_async_call`, so a screen is never
deleted inside its own event handler.

Large network responses reserve their buffer in one allocation up front
(see `sizeHint` in `StarlinkTransport`). Allocation failures are caught
rather than aborting.

## Rendering cost

Rough frame times at 240 MHz: switching page (build plus full render)
150 - 270 ms; a history refresh about 70 ms once per 2 s; dashboard
updates under 30 ms. Graph lines are drawn as axis-aligned 2 px bars, one
per pixel column. LVGL's anti-aliased diagonal lines were about twice as
slow.

## Flash budget

The app partition is 1.9 MB (`min_spiffs.csv`, two OTA slots). Milestone 8
uses about 1.64 MB (83%). The WiFi/TCP/WPA stack accounts for roughly 530 KB and
LVGL for about 300 KB. If space gets tight, disable unused LVGL widgets in
`lv_conf.h` first.
