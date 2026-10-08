#pragma once

// Project identity reported over serial, on screen, in the web UI and (later)
// for OTA checks. The version is not edited here: it comes from git via
// scripts/version.py (tag v0.1.0 -> "0.1.0", otherwise
// "<VERSION file>-dev+<commit>"). See docs/development/release.md.

#if __has_include("build_info.h")
#include "build_info.h"
#endif

#ifndef SM_VERSION
#define SM_VERSION "0.0.0-unknown"
#endif
#ifndef SM_GIT_COMMIT
#define SM_GIT_COMMIT "unknown"
#endif
#ifndef SM_BOARD_ID
#define SM_BOARD_ID "unknown"
#endif

#define SM_PROJECT_NAME "Starlink Monitor"
#define SM_BUILD_DATE   __DATE__ " " __TIME__
