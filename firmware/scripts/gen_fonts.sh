#!/bin/sh
# Regenerates the digit-only display fonts in src/ui/fonts/.
# Requires Node.js (npx downloads lv_font_conv). The generated .c files are
# committed, so this is only needed when changing sizes or glyph ranges.
set -e
cd "$(dirname "$0")/.."
TTF=.pio/libdeps/cyd_2432s028_st7789/lvgl/scripts/built_in_font/Montserrat-Medium.ttf
[ -f "$TTF" ] || { echo "Run 'pio pkg install' first (needs LVGL's bundled Montserrat)"; exit 1; }
# space % + - . / 0-9 :
RANGE=0x20,0x25,0x2B,0x2D-0x3A
for SIZE in 48 36; do
  npx --yes lv_font_conv@1.5.3 --font "$TTF" -r "$RANGE" --size "$SIZE" --bpp 4 \
    --no-compress --format lvgl --lv-include lvgl.h --lv-font-name "sm_font_num_$SIZE" \
    -o "src/ui/fonts/sm_font_num_$SIZE.c"
done
