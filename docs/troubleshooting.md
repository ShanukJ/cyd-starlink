# Troubleshooting

## Installing

**No serial port shows up in the installer**
- Try another USB cable. Many cables only charge.
- Install the CH340 USB-serial driver (Windows and older macOS).
- On dual-USB boards, use the micro-USB port.
- Close anything else that has the port open (serial monitor, Arduino IDE,
  PlatformIO).

**"Failed to connect" / stuck at "Connecting…"**
Hold the **BOOT** button, start the install, and release BOOT once writing
begins.

**Blank or garbled screen after installing**
You may have the ILI9341 version of the board, which isn't supported yet.
See [which board do I have](hardware/README.md#which-esp32-2432s028-do-i-have).

## WiFi

**The setup network doesn't appear**
Setup only opens when no network is saved, when the saved one fails 3
times after power-up, or when you hold BOOT for 3 seconds. The screen
shows the network name and password whenever setup is open.

**The setup page doesn't open by itself**
Browse to `http://192.168.4.1` while connected to the setup network. Some
phones need you to tap the "Sign in to network" notification.

**It keeps saying "Can't join …"**
The error after the network name tells you why:
- `AUTH_FAIL` / `4WAY_HANDSHAKE_TIMEOUT`: wrong password.
- `NO_AP_FOUND`: wrong network name, out of range, or a 5 GHz-only
  network. The ESP32 supports 2.4 GHz only.

**`http://starlink-monitor.local/` doesn't open**
Some networks and Android versions don't support `.local` names. Use the
IP address from the diagnostics page instead.

## Starlink

**OFFLINE · Dish unreachable**
The monitor reaches the dish at `192.168.100.1` port 9201.
- The monitor must be on a network that routes to the dish, normally the
  Starlink router's WiFi. A third-party router in front of Starlink may
  need a route to `192.168.100.0/24`.
- Check the dish address on the settings page.

**ERROR · Not a Starlink dish**
Something answered at the configured address, but it isn't a dish. Check
the dish address.

**Some values show `--`**
The dish didn't report them, or reported something invalid. The monitor
shows `--` rather than guessing. Older dish firmware may not report every
field (for example signal quality).

## Getting logs

Connect at 115200 baud (for example `pio device monitor`). Normal
operation logs only changes. Include the boot log when reporting an
issue, and remove your WiFi network name if you prefer.
