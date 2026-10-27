<!-- Thanks for contributing! See CONTRIBUTING.md. Delete anything that doesn't apply. -->

## Summary

<!-- What does this change and why? Link the issue it fixes, e.g. "Fixes #12". -->

## Testing

<!-- How did you verify it? Tick what you did and add details. -->

- [ ] `pio test -e native` passes
- [ ] Firmware builds for every board env (`pio run`)
- [ ] Flashed and checked on real hardware (which board?)
- [ ] Tested against a real Starlink dish (dish hardware / software version?)

Details:

## Affected hardware profiles

<!-- e.g. esp32-2432s028-st7789, "all", or "none (docs / host-only code)". -->

## Firmware compatibility

- [ ] No change to saved settings (NVS) or their meaning
- [ ] Settings schema changed and old settings migrate (bump the schema in `config/Settings.cpp`)
- [ ] Changes the web UI / JSON API or the serial commands
- [ ] Flash or RAM use changes noticeably (give before / after numbers, including free heap)

## Screenshots

<!-- For UI changes: before / after, portrait and landscape where relevant
     (firmware/scripts/screenshot.py). No WiFi names, IDs or locations. -->

## Checklist

- [ ] Follows the ground rules in CONTRIBUTING.md (no guessed pins, no fake zeros, no blocking the UI task, no cloud)
- [ ] Docs updated if behaviour or setup changed
- [ ] No credentials, device IDs or other personal data in code, logs or screenshots
