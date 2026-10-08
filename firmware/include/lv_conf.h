/**
 * LVGL 9.5 configuration for Starlink Monitor.
 *
 * Only options that differ from LVGL's defaults are set here; everything
 * else falls back to lv_conf_internal.h. Keep this file short so it's
 * obvious what the project actually depends on.
 */

#ifndef LV_CONF_H
#define LV_CONF_H

/* RGB565: native format of the SPI panels we support. */
#define LV_COLOR_DEPTH 16

/* Use the ESP-IDF heap instead of a fixed LVGL pool: no PSRAM on the
 * CYD, so a static pool would permanently reserve scarce DRAM. */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* LVGL is only ever called from the UI task. */
#define LV_USE_OS LV_OS_NONE

#define LV_DEF_REFR_PERIOD 20
#define LV_DPI_DEF 140

/* Warnings and errors go to Serial via lv_log_register_print_cb(). */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0

/* Fonts. Each Montserrat size costs flash, so only enable what the UI uses. */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* QR code widget: WiFi setup screen. */
#define LV_USE_QRCODE 1

/* Don't compile LVGL's bundled examples into the firmware. */
#define LV_BUILD_EXAMPLES 0

#endif /*LV_CONF_H*/
