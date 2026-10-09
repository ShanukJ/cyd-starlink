# Releasing

## Versions

- The base version lives in the root `VERSION` file (for example `0.1.0`).
- `firmware/scripts/version.py` derives the version every build reports
  (boot log, web UI, `/api/status`):
  - a clean checkout of tag `v0.1.0` reports `0.1.0`;
  - anything else reports `0.1.0-dev+<commit>`, plus `.dirty` with
    uncommitted changes.
- The version, commit and board ID are written to a generated header
  (`.pio/generated/build_info.h`), so changing them only rebuilds the few
  files that use them.

Use [semantic versioning](https://semver.org). Bump the minor version
when the settings schema changes.

## Building release files locally

```sh
cd firmware
~/.platformio/penv/bin/python scripts/release.py
```

Every PlatformIO env with a `custom_board_id` is built into `dist/`:

```
dist/firmware/
  starlink-monitor-<ver>-<board>-factory.bin   offset 0x0, erases settings
  starlink-monitor-<ver>-<board>-app.bin       offset 0x10000, keeps settings
  <board>/{bootloader,partitions,boot_app0,firmware}.bin   web-flasher parts
  <board>.manifest.json                        ESP Web Tools manifest
  boards.json, SHA256SUMS
dist/site/                                     web flasher + firmware (deployed to GitHub Pages)
```

Flash offsets come from the platform's own upload configuration
(`scripts/factory_image.py`), never hard-coded.

**Why separate parts in the flasher?** A merged image fills the gaps
between regions with `0xFF`, including the NVS settings partition at
`0x9000`. Writing the parts separately means an install without erase
keeps the user's settings.

To try the flasher locally (Web Serial needs `localhost` or HTTPS):

```sh
cd dist/site && python3 -m http.server 8000   # open http://localhost:8000 in Chrome
```

## Cutting a release

1. Update `VERSION` and `CHANGELOG.md`, and commit.
2. Tag and push:
   ```sh
   git tag v0.1.0 && git push origin main v0.1.0
   ```
3. CI ([.github/workflows/ci.yml](../../.github/workflows/ci.yml)):
   1. runs the host tests;
   2. checks the tag matches `VERSION`;
   3. builds every board;
   4. creates the GitHub Release with the `.bin` files and `SHA256SUMS`;
   5. deploys `dist/site` (the web flasher with its firmware) to GitHub
      Pages.

The flasher has to serve the firmware itself: browsers can't download
GitHub Release assets from another site.

### GitHub Pages setup (one time)

1. **Settings → Pages → Source: GitHub Actions.**
2. **Settings → Environments → `github-pages` → Deployment branches and
   tags → Add rule → Tag, pattern `v*`.** The `github-pages` environment
   only accepts deployments from `main` by default. Without this rule a
   tag push builds and releases, but the Pages deploy is rejected with
   *"Tag … is not allowed to deploy to github-pages due to environment
   protection rules"*.

### Redeploying the flasher

To (re)deploy the flasher for an existing release, go to **Actions → CI →
Run workflow**, keep the branch set to `main`, and enter the tag
(`v0.1.0`). That run builds the tagged code, so it reports the release
version, and deploys it to Pages. It doesn't create a GitHub Release.
Because it runs from `main`, it works even without the tag rule above.

## Hardware check before tagging

CI can't test hardware. Before tagging, flash the release candidate and
check that:

1. it boots, the dashboard shows data, and swiping works;
2. updating from the previous release through the web flasher, without
   erase, keeps the settings;
3. the factory image boots into setup mode.
