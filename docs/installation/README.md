# Installing Starlink Monitor

You need a [supported board](../hardware/README.md), a USB **data** cable
(some cables only charge), and a WiFi network that can reach your Starlink
dish. That is usually the Starlink router's own network.

## Option 1: web installer (recommended)

1. Open the web installer, **[cyd-starlink.shanukj.me](https://cyd-starlink.shanukj.me)**,
   in **Chrome or Edge on a computer**. It is updated with every release.
   Safari, Firefox and phones can't access USB serial ports.
2. Choose your board.
3. Plug the board in, click **Connect**, pick its serial port (often
   "USB Serial" or "CH340") and choose **Install**.
   - For a **first install**, let it erase the device.
   - For an **update**, choose *not* to erase. Your WiFi and other
     settings are kept.
4. Wait about a minute. The board restarts into setup (see below).

## Option 2: esptool

Each [release](https://github.com/ShanukJ/cyd-starlink/releases) has two images per board:

| File | Flash at | Use |
|---|---|---|
| `starlink-monitor-<version>-<board>-factory.bin` | `0x0` | new install. **Erases all settings** (WiFi included). |
| `starlink-monitor-<version>-<board>-app.bin` | `0x10000` | update. Keeps settings. |

```sh
pip install esptool
esptool --chip esp32 --port /dev/ttyUSB0 write-flash 0x0 starlink-monitor-0.1.0-esp32-2432s028-st7789-factory.bin
```

Check the download against `SHA256SUMS` from the same release.

## Option 3: build it yourself

```sh
git clone <this repository>
cd starlink-monitor/firmware
pio run -e cyd_2432s028_st7789 -t upload
```

See [CONTRIBUTING.md](../../CONTRIBUTING.md) for the development setup.

## First setup

1. The screen shows a WiFi network called `STARLINK-MONITOR-XXXX`, a
   password and a QR code.
2. Scan the QR code with your phone, or join the network by hand.
3. The setup page opens by itself. If it doesn't, browse to
   `http://192.168.4.1`.
4. Pick the WiFi network that can reach your dish, enter its password, and
   keep the dish address at `192.168.100.1` unless yours is different.
   Save.

The monitor connects and shows the dashboard. From then on, change
settings at `http://starlink-monitor.local/` or at the IP address shown on
the diagnostics page.

To reopen WiFi setup later, **hold the BOOT button for 3 seconds**.

If anything goes wrong, see [troubleshooting](../troubleshooting.md).
