# Starlink Monitor

A standalone Starlink status display for cheap ESP32 touchscreen boards
("Cheap Yellow Display"). It talks directly to your Starlink dish on your
local network. No cloud, no Starlink account, no Home Assistant, no MQTT,
no extra server.

<p>
  <img src="docs/images/dashboard-portrait.png" width="240" alt="Dashboard: state, download, upload, latency, obstruction, signal, uptime">
  <img src="docs/images/history-portrait.png" width="240" alt="History: 15-minute throughput and latency graphs">
  <img src="docs/images/alignment-portrait.png" width="240" alt="Alignment: dish vs target pointing with guidance">
  <img src="docs/images/diagnostics-portrait.png" width="240" alt="Diagnostics: per-subsystem checks and versions">
</p>

> **Status: v0.1.0 release candidate.** Runs on the ESP32-2432S028 (ST7789
> version), tested against a real dish.

## Features

- **Dashboard:** dish state (ONLINE, DEGRADED, OFFLINE, ERROR,
  CONNECTING), shown as a word and a symbol, not just a colour. Also
  current download and upload, each with its 15-minute peak, plus latency,
  obstruction, signal quality and uptime. The dish reports traffic actually
  flowing, not link capacity: an idle connection shows a few kbps, and the
  peak shows what it carried recently.
- **History:** 15-minute throughput and latency graphs, filled from the
  dish's own history the moment the monitor starts.
- **Alignment:** where the dish points versus where Starlink wants it to,
  with plain guidance ("Turn 13° clockwise · Raise 4.2°").
- **Diagnostics:** hardware self-test, RF, GPS, network, thermal,
  obstruction, Ethernet, alerts and software update, plus versions.
- **Honest data:** a value the dish doesn't report shows as `--`, never as
  a made-up 0. If the dish disappears, you see how long ago it was last
  seen, and the monitor keeps retrying.
- **Browser settings** at `http://starlink-monitor.local/`, and WiFi setup
  from your phone with a QR code.
- Portrait or landscape. Swipe between pages, or tap the page dots.

## Hardware

| Board | Status |
|---|---|
| ESP32-2432S028, **ST7789** display, XPT2046 touch | ✅ supported |
| ESP32-2432S028, ILI9341 display | not yet |

This board is sold with two different display chips:
[how to tell which one you have](docs/hardware/README.md).

## Install

Use the **web installer** in Chrome or Edge on a computer: plug the
board in over USB, pick it from the list and click **Connect**. No
software to install. Release images for esptool are also available.

→ [Installation guide](docs/installation/README.md)

## First setup

1. The screen shows a WiFi network `STARLINK-MONITOR-XXXX`, a password and
   a QR code.
2. Join it with your phone. The setup page opens (or go to
   `http://192.168.4.1`).
3. Pick the WiFi network that can reach your dish (usually the Starlink
   router's) and save.

The setup network uses WPA2 with a new random password each time. The
password is only shown on the device's own screen.

## Using it

| Action | How |
|---|---|
| Switch page | swipe left/right, or tap the dots at the bottom |
| Settings | `http://starlink-monitor.local/`, or the IP on the diagnostics page |
| Reopen WiFi setup | hold **BOOT** for 3 seconds |
| Hardware test screen | long-press the dashboard for 1.5 seconds |

The settings page covers WiFi, dish address, refresh interval (1 or 2 s),
brightness, orientation, restart and factory reset. There is no login:
anyone on your local network can open it, as with most home appliances.
Other websites can't change settings through your browser, and the
monitor only answers to its own name or IP address.

## Privacy

The monitor only talks to your dish and to devices on your own network.
It sends nothing to the internet. It doesn't use the internet at all,
including for time, so "last seen" is shown as "2 min ago" rather than a
clock time.

## Documentation

- [Installation](docs/installation/README.md) · [Troubleshooting](docs/troubleshooting.md)
- [Supported hardware](docs/hardware/README.md) · [ESP32-2432S028 ST7789 details](docs/hardware/esp32-2432s028-st7789.md)
- [Architecture](docs/development/architecture.md) · [Data model](docs/development/data-model.md) ·
  [Starlink local API notes](docs/protocol/starlink-grpc-web.md) · [Releasing](docs/development/release.md)
- [Contributing](CONTRIBUTING.md) · [Changelog](CHANGELOG.md)

## Building from source

```sh
cd firmware
pio run -e cyd_2432s028_st7789 -t upload   # build + flash (PlatformIO)
pio test -e native                         # host unit tests
```

See [CONTRIBUTING.md](CONTRIBUTING.md) for the development setup, the
serial developer commands (`screenshot`, `page`, `rotate`) and how to add a
board.

## License

MIT. See [LICENSE](LICENSE). Uses [LVGL](https://lvgl.io) (MIT),
[LovyanGFX](https://github.com/lovyan03/LovyanGFX) (MIT/BSD) and the
Montserrat font (SIL OFL 1.1). The web installer uses
[ESP Web Tools](https://esphome.github.io/esp-web-tools/) (Apache 2.0).
Not affiliated with SpaceX or Starlink.
