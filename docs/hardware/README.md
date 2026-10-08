# Supported hardware

| Board | Display | Touch | Status | Firmware |
|---|---|---|---|---|
| ESP32-2432S028 ("Cheap Yellow Display"), **ST7789** version | 2.8" 240×320 ST7789 | XPT2046 resistive | ✅ tested on hardware | `esp32-2432s028-st7789` |
| ESP32-2432S028, ILI9341 version | 2.8" 240×320 ILI9341 | XPT2046 resistive | ❌ not supported yet | — |
| ESP32-2432S024, -2432S032, -3248S035 and others | various | various | ❌ not supported yet | — |

Details for each supported board, including pin mapping, sources and
measured settings, are in this folder: [ESP32-2432S028 ST7789](esp32-2432s028-st7789.md).

## Which ESP32-2432S028 do I have?

The ESP32-2432S028 is sold with two different display controllers on
the same circuit board. The firmware for one does not show a usable
picture on the other. Ways to tell them apart:

1. **The listing or box.** Sellers often state "ST7789" or "ILI9341". A
   "7789" on the box means ST7789.
2. **The USB ports are not a reliable sign.** Many dual-port boards
   (micro-USB plus USB-C) are ST7789, but not all.
3. **Ask the board.** Install the ST7789 firmware. If the screen stays
   blank or shows garbage while the serial log (115200 baud) shows normal
   boot messages, it is probably the ILI9341 version. On an ST7789 board
   the log reports the panel as responding:
   ```
   [DISPLAY] Module ID 81 81 B3, power mode 0x9C (on), pixel format 0x05
   ```
   Module IDs vary between panel makers. The `power mode 0x9C (on)` part
   is what matters.

Nothing is damaged by trying the wrong firmware. Flash the right one
afterwards.

## Adding a board

See [CONTRIBUTING.md](../../CONTRIBUTING.md#adding-a-board). A new board
needs a hardware profile, a PlatformIO environment and, if it uses a new
display or touch controller, a driver. No Starlink or screen code
changes.
