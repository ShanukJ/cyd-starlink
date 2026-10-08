#!/usr/bin/env python3
"""Build every board target and assemble the release in dist/.

    python3 scripts/release.py [--version 0.1.0] [--skip-build]

Run from the firmware/ directory with a Python that has PlatformIO
(e.g. ~/.platformio/penv/bin/python). Produces:

    dist/firmware/starlink-monitor-<ver>-<board>-factory.bin   everything, flash at 0x0 (erases settings)
    dist/firmware/starlink-monitor-<ver>-<board>-app.bin       app only, flash at 0x10000 (keeps settings)
    dist/firmware/<board>/                                      separate parts for the web flasher
    dist/firmware/<board>.manifest.json                         ESP Web Tools manifest (uses the parts, so
                                                                an install without erase keeps settings)
    dist/firmware/boards.json                                   board list for the flasher page
    dist/firmware/SHA256SUMS
    dist/site/                                                  web flasher + firmware, ready for GitHub Pages

Board targets are the platformio.ini envs that set custom_board_id.
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys

FIRMWARE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO_DIR = os.path.dirname(FIRMWARE_DIR)
CHIP_FAMILY = {"esp32": "ESP32", "esp32s3": "ESP32-S3", "esp32s2": "ESP32-S2", "esp32c3": "ESP32-C3"}


def pio(*args, env=None):
    return subprocess.run([sys.executable, "-m", "platformio", *args], cwd=FIRMWARE_DIR, env=env, check=True,
                          capture_output=True, text=True).stdout


def board_targets():
    config = json.loads(pio("project", "config", "--json-output"))
    targets = []
    for section, options in config:
        if not section.startswith("env:"):
            continue
        opts = dict(options)
        if "custom_board_id" not in opts:
            continue
        targets.append({
            "env": section[4:],
            "id": opts["custom_board_id"],
            "name": opts.get("custom_board_name", opts["custom_board_id"]),
            "board": opts.get("board"),
        })
    return targets


def mcu_of(board):
    # `pio boards <q>` is a search: pick the exact id.
    data = json.loads(pio("boards", board, "--json-output"))
    return next(b for b in data if b["id"] == board)["mcu"].lower()


def repo_url():
    """https://github.com/<owner>/<repo>, from CI or the git remote; None if unknown."""
    if os.environ.get("GITHUB_REPOSITORY"):
        return f"https://github.com/{os.environ['GITHUB_REPOSITORY']}"
    try:
        url = subprocess.check_output(["git", "remote", "get-url", "origin"], cwd=REPO_DIR,
                                      stderr=subprocess.DEVNULL, text=True).strip()
    except Exception:
        return None
    m = re.match(r"(?:git@github\.com:|https://github\.com/)([^/]+/[^/]+?)(?:\.git)?$", url)
    return f"https://github.com/{m.group(1)}" if m else None


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", help="override the git-derived version (CI sets this from the tag)")
    ap.add_argument("--skip-build", action="store_true", help="package existing builds only")
    ap.add_argument("--out", default=os.path.join(REPO_DIR, "dist"))
    args = ap.parse_args()

    env = dict(os.environ)
    if args.version:
        env["SM_VERSION"] = args.version

    targets = board_targets()
    if not targets:
        sys.exit("No envs with custom_board_id in platformio.ini")

    out_fw = os.path.join(args.out, "firmware")
    shutil.rmtree(args.out, ignore_errors=True)
    os.makedirs(out_fw)

    version = None
    boards = []
    for t in targets:
        print(f"== {t['env']} ({t['name']})", flush=True)
        if not args.skip_build:
            pio("run", "-e", t["env"], "-t", "factory", env=env)
        build = os.path.join(FIRMWARE_DIR, ".pio", "build", t["env"])
        with open(os.path.join(FIRMWARE_DIR, ".pio", "generated", "build_info.h")) as f:
            v = re.search(r'SM_VERSION "([^"]+)"', f.read()).group(1)
        if version and v != version:
            sys.exit(f"Version mismatch between targets: {version} vs {v}")
        version = v

        # File names avoid "+" (dev versions): it is mangled in some URLs.
        base = f"starlink-monitor-{version.replace('+', '-')}-{t['id']}"
        factory = f"{base}-factory.bin"
        app = f"{base}-app.bin"
        shutil.copy(os.path.join(build, "factory.bin"), os.path.join(out_fw, factory))
        shutil.copy(os.path.join(build, "firmware.bin"), os.path.join(out_fw, app))

        with open(os.path.join(build, "flash_layout.json")) as f:
            layout = json.load(f)
        parts_dir = os.path.join(out_fw, t["id"])
        os.makedirs(parts_dir)
        parts = []
        for part in layout["parts"]:
            name = os.path.basename(part["path"])
            shutil.copy(part["path"], os.path.join(parts_dir, name))
            parts.append({"path": f"{t['id']}/{name}", "offset": part["offset"]})

        manifest = {
            "name": f"Starlink Monitor for {t['name']}",
            "version": version,
            "new_install_prompt_erase": True,
            "builds": [{"chipFamily": CHIP_FAMILY[mcu_of(t["board"])], "parts": parts}],
        }
        manifest_name = f"{t['id']}.manifest.json"
        with open(os.path.join(out_fw, manifest_name), "w") as f:
            json.dump(manifest, f, indent=2)

        boards.append({"id": t["id"], "name": t["name"], "manifest": manifest_name, "factory": factory, "app": app,
                       "factory_sha256": sha256(os.path.join(out_fw, factory))})
        print(f"   {factory} ({os.path.getsize(os.path.join(out_fw, factory))} bytes)")

    with open(os.path.join(out_fw, "boards.json"), "w") as f:
        json.dump({"version": version, "repo": repo_url(), "boards": boards}, f, indent=2)
    with open(os.path.join(out_fw, "SHA256SUMS"), "w") as f:
        for name in sorted(os.listdir(out_fw)):
            if name.endswith(".bin"):
                f.write(f"{sha256(os.path.join(out_fw, name))}  {name}\n")
        for t in targets:  # web-flasher parts
            for name in sorted(os.listdir(os.path.join(out_fw, t["id"]))):
                f.write(f"{sha256(os.path.join(out_fw, t['id'], name))}  {t['id']}/{name}\n")

    site = os.path.join(args.out, "site")
    shutil.copytree(os.path.join(REPO_DIR, "web", "flasher"), site)
    shutil.copytree(out_fw, os.path.join(site, "firmware"))
    print(f"\nRelease {version}: {len(boards)} board(s) -> {args.out}")


if __name__ == "__main__":
    main()
