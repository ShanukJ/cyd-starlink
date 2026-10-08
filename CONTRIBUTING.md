# Contributing

Thanks for helping. This project aims to be a dependable appliance, so
changes need to stay small, readable and tested on real hardware where
hardware is involved.

## Development setup

- [PlatformIO](https://platformio.org/) (CLI or the VS Code extension)
- a supported board, for anything that touches hardware or the UI
- optionally a Starlink dish on your network. `grpcurl -plaintext
  192.168.100.1:9200 list` works if you can reach it.

```sh
cd firmware
pio run -e cyd_2432s028_st7789 -t upload   # build + flash
pio device monitor                         # 115200 baud log
pio test -e native                         # host unit tests (ASan/UBSan)
```

The serial console accepts developer commands: `screenshot` (see
`scripts/screenshot.py`), `page <n>` and `rotate <r>`. Use them to check UI
changes and to produce screenshots for pull requests.

## Ground rules

These come from the project's design and are checked in review:

1. **No faked hardware or API behaviour.** Verify on the device or dish,
   and document what you observed.
2. **No guessed pinouts.** Cite sources in the hardware profile and in
   `docs/hardware/`.
3. **Missing telemetry is never a zero.** Use `std::optional` / `--`. See
   [data-model.md](docs/development/data-model.md).
4. **Keep layers apart:** hardware ↔ Starlink client ↔ data model ↔ UI.
   The UI never sees the transport or protobuf.
5. **Never block the UI task** with network I/O. Network work runs on the
   network task, and the UI reads snapshots.
6. **No cloud services, MQTT, Home Assistant or outgoing telemetry.**
7. **Watch the heap.** There is no PSRAM. Measure free heap and the
   largest free block before and after your change, and include the
   numbers in the pull request if they move.
8. **No private data** in fixtures, logs or screenshots: dish/router IDs,
   WiFi names, locations.

Pure logic (decoders, health, formatting, validation) lives in files
without Arduino dependencies and gets host tests in `firmware/test/`. New
logic of that kind needs tests.

## Adding a board

1. Add `firmware/src/hardware/profiles/<board>.h` with a `HardwareProfile`.
   Document pins, controllers, colour order/inversion and touch
   calibration, and cite your sources.
2. Add a `HW_PROFILE_<BOARD>` branch to `profiles/Profiles.h`.
3. Add an `[env:<board>]` to `firmware/platformio.ini`. It extends `esp32`
   and sets `custom_board_id` and `custom_board_name`. That is all the
   release script and web flasher need.
4. New display or touch controller? Add a driver behind `DisplayDriver` /
   `TouchDriver`. A new LovyanGFX panel type is usually one `case` in
   `LgfxSpiDisplay::begin`.
5. Verify on the hardware with the hardware test screen (long-press the
   dashboard): colour bars, touch crosshair in all four rotations,
   backlight. Record the results in `docs/hardware/<board>.md` and the
   compatibility table.

## Pull requests

- One topic per pull request, with commits that each make sense on their
  own.
- Say how you tested it: host tests, which board, and whether it ran
  against a real dish.
- Include screenshots for UI changes (`scripts/screenshot.py`).
- Code style: match the surrounding code. 4-space indents, `_member`
  fields, comments explain *why*.

## Reporting bugs

Include the boot log (115200 baud), the firmware version (boot log or the
web page's Device section), your board, and the dish's hardware and
software version from the diagnostics page.
