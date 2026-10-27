# Security policy

## Reporting a vulnerability

**Please don't report security problems in public issues, pull requests
or discussions.**

Report them privately through GitHub:
**[Report a vulnerability](https://github.com/ShanukJ/cyd-starlink/security/advisories/new)**
(Security tab → *Report a vulnerability*). Only the maintainers can see
the report.

Please include:

- what the problem is and what an attacker could do with it;
- the firmware version (boot log or the web page's Device section) and the
  board;
- steps to reproduce, or a proof of concept;
- whether it needs physical access, local network access, or neither.

## What to expect

This is a volunteer project, so these are best-effort targets:

- an acknowledgement within **7 days**;
- an assessment, and a plan or a fix, within **30 days** for confirmed
  issues;
- a fix released as a new firmware version, with credit in the release
  notes if you want it.

Please give us a reasonable chance to release a fix before disclosing
the problem publicly.

## Supported versions

Only the latest release gets security fixes. Update with the
[web installer](https://cyd-starlink.shanukj.me) (choose *not* to erase,
to keep your settings).

## Scope

In scope:

- the firmware: the web settings page and its JSON API, the WiFi setup
  portal, the setup access point, and parsing of data received from the
  dish;
- the web installer site and the release pipeline (GitHub Actions,
  release files).

By design, and not vulnerabilities on their own:

- **The settings page has no login.** Anyone on the same local network can
  open it and change settings, as with most home appliances. It is
  protected against cross-site request forgery (a required custom
  header) and DNS rebinding (the device only answers to its own names
  and addresses), and it never returns the WiFi password. Ways around
  *those* protections are in scope.
- **Physical access means full control.** Anyone with the board and a USB
  cable can read or replace the firmware and its stored settings,
  including the WiFi password.
- **The setup network's password is shown on the device's screen.** That
  is how setup works.
- **The Starlink dish's own local API** is SpaceX's, not this project's.

The monitor makes no internet connections. It only talks to the dish and
to devices on your local network. Anything that makes it contact other
hosts is a bug worth reporting.

Problems in third-party libraries (LVGL, LovyanGFX, the ESP32 Arduino
core, ESP Web Tools) should go to those projects. Tell us as well if this
firmware is affected, so we can update.
