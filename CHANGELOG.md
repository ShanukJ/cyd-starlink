# Changelog

All notable changes are listed here. Versions follow
[semantic versioning](https://semver.org).

## [0.1.0] - 2026-10-09

First release. Supports the ESP32-2432S028 ("Cheap Yellow Display")
**ST7789** version.

### Display
- Dashboard: state (ONLINE / DEGRADED / OFFLINE / ERROR / CONNECTING,
  shown as a symbol and a word), current download and upload with their
  15-minute peaks, latency, obstruction, signal and uptime. Portrait and
  landscape layouts.
- History: 15-minute throughput and latency graphs, filled from the dish's
  own history right after power-up.
- Alignment: current vs desired dish pointing on a top-down plot, with
  plain guidance.
- Diagnostics: nine per-subsystem checks, plus dish and monitor versions.
- Swipe between pages. Long-press opens a hardware test screen.

### Connection
- Talks directly to the dish over its local gRPC-Web API (port 9201). No
  cloud, account, MQTT or Home Assistant needed.
- Missing telemetry is shown as `--`, never as a made-up zero.
- Retries with backoff, and shows "last seen" while the dish is
  unreachable.

### Setup and settings
- First-time WiFi setup through a WPA2 setup network with a QR code and a
  captive portal.
- Settings page at `http://starlink-monitor.local/`: WiFi, dish address,
  refresh interval, brightness, orientation, restart, factory reset.
  Protected against cross-site requests and DNS rebinding.

### Releases
- Web installer (ESP Web Tools). Updates without erasing keep your
  settings.
- Release images per board with SHA-256 checksums.
