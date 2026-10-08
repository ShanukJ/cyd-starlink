# PlatformIO post-script: adds a "factory" target that
#   - merges bootloader, partition table, boot_app0 and the app into one
#     image for offset 0 (manual `esptool write-flash 0x0` installs). The
#     gaps are 0xFF-filled, so this image also ERASES the NVS settings;
#   - writes flash_layout.json listing the separate parts and offsets, which
#     the web flasher uses so that updates can keep the settings.
# Offsets come from the platform's own upload configuration, never hard-coded.
#
#   pio run -e <env> -t factory   ->  .pio/build/<env>/factory.bin, flash_layout.json
import json
import os
import sys

Import("env")  # noqa: F821


def build_factory(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    out = os.path.join(build_dir, "factory.bin")
    images = [(offset, env.subst(path)) for offset, path in env.get("FLASH_EXTRA_IMAGES", [])]
    images.append((env.subst("$ESP32_APP_OFFSET"), os.path.join(build_dir, "firmware.bin")))
    board = env.BoardConfig()
    cmd = [
        "--chip", board.get("build.mcu"),
        "merge-bin", "-o", out,
        "--flash-mode", board.get("build.flash_mode", "dio"),
        "--flash-size", board.get("upload.flash_size", "4MB"),
    ]
    for offset, path in sorted(images, key=lambda i: int(i[0], 16)):
        cmd += [offset, path]
    print("Merging:", ", ".join(f"{o} {os.path.basename(p)}" for o, p in sorted(images, key=lambda i: int(i[0], 16))))
    # $OBJCOPY is the esptool executable on this platform (already quoted if needed).
    if env.Execute(env.subst("$OBJCOPY") + " " + " ".join(f'"{c}"' for c in cmd)):
        sys.exit(1)
    print(f"Factory image: {out} ({os.path.getsize(out)} bytes)")
    layout = [{"offset": int(o, 16), "path": p} for o, p in sorted(images, key=lambda i: int(i[0], 16))]
    with open(os.path.join(build_dir, "flash_layout.json"), "w") as f:
        json.dump({"chip": board.get("build.mcu"), "parts": layout}, f, indent=2)


env.AddCustomTarget(  # noqa: F821
    name="factory",
    dependencies="$BUILD_DIR/${PROGNAME}.bin",
    actions=build_factory,
    title="Factory image",
    description="Merge bootloader, partitions and app into one image at offset 0",
)
