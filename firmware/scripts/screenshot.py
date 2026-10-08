#!/usr/bin/env python3
"""Capture the monitor's screen over USB serial and save it as a PNG.

    python3 scripts/screenshot.py /dev/cu.usbserial-XXXX out.png [--scale 2]

Needs pyserial (PlatformIO's Python has it:
~/.platformio/penv/bin/python scripts/screenshot.py ...). Close any serial
monitor first. Some USB-serial drivers (CH340 on macOS) reset the board when
the port opens regardless of DTR/RTS; the script then waits for boot.
"""

import argparse
import struct
import sys
import time
import zlib

import serial


def read_until(port, marker, timeout):
    buf = b""
    end = time.time() + timeout
    while time.time() < end:
        buf += port.read(max(1, port.in_waiting))
        i = buf.find(marker)
        if i >= 0:
            return buf[i + len(marker):]
    raise TimeoutError(f"no {marker!r} from device")


def write_png(path, w, h, rgb_rows):
    def chunk(kind, data):
        c = kind + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)

    raw = b"".join(b"\x00" + row for row in rgb_rows)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("port")
    ap.add_argument("out")
    ap.add_argument("--scale", type=int, default=1, help="integer upscale for viewing")
    args = ap.parse_args()

    port = serial.Serial()
    port.port, port.baudrate, port.timeout = args.port, 115200, 0.2
    port.dtr = False  # don't reset the board on open
    port.rts = False
    port.open()

    # If opening the port rebooted the board, wait until the UI is up.
    boot = b""
    end = time.time() + 1.5
    while time.time() < end:
        boot += port.read(4096)
    if b"rst:" in boot:
        print("board rebooted on port open; waiting for it to start...")
        if b"[BOOT] Ready" not in boot:
            read_until(port, b"[BOOT] Ready", 20)
        time.sleep(4)  # let WiFi/dish data arrive so the screen isn't empty
    port.reset_input_buffer()
    port.write(b"\nscreenshot\n")

    rest = read_until(port, b"#SCREENSHOT ", 10)
    while b"\n" not in rest:
        rest += port.read(64)
    header, rest = rest.split(b"\n", 1)
    w, h = (int(v) for v in header.split())
    need = w * h * 2
    data = bytearray(rest)
    end = time.time() + 60
    while len(data) < need and time.time() < end:
        data += port.read(need - len(data))
    port.close()
    if len(data) < need:
        sys.exit(f"short read: {len(data)}/{need} bytes")

    rows = []
    for y in range(h):
        row = bytearray()
        for x in range(w):
            v = data[(y * w + x) * 2] | data[(y * w + x) * 2 + 1] << 8
            r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
            px = bytes(((r * 255 + 15) // 31, (g * 255 + 31) // 63, (b * 255 + 15) // 31))
            row += px * args.scale
        rows.extend([bytes(row)] * args.scale)
    write_png(args.out, w * args.scale, h * args.scale, rows)
    print(f"saved {args.out} ({w}x{h}, x{args.scale})")


if __name__ == "__main__":
    main()
