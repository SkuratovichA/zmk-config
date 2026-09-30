/*
 * SPDX-License-Identifier: MIT
 *
 * The Kconfig symbols of the Kyria's OLED build (the central), force-included with -include
 * instead of harness_config.h for the oled variant: the same split-central symbols, the keyboard
 * name of the Kyria and the clock settings of the OLED shield. Not applied to LVGL, which is
 * configured by autoconf_lv.h and lv_conf_host.h.
 *
 * CONFIG_BESIM_DISPLAY_INVERTED is left undefined on purpose: the OLED shows the same glyph
 * bitmaps as the Lily, and the polarity is decided by the display, not by the sources.
 */

#pragma once

#define CONFIG_ZMK_SPLIT 1
#define CONFIG_ZMK_SPLIT_ROLE_CENTRAL 1
#define CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING 1
#define CONFIG_ZMK_BLE 1
#define CONFIG_USB_DEVICE_STACK 1
#define CONFIG_ZMK_KEYBOARD_NAME "Kyria"
#define CONFIG_ZMK_LOG_LEVEL 0
#define CONFIG_BT_MAX_PAIRED 5

#define CONFIG_KYRIA_OLED_CLOCK 1
#define CONFIG_KYRIA_OLED_CLOCK_IDLE_SECONDS 300
#define CONFIG_KYRIA_OLED_CLOCK_CONTRAST 16
#define CONFIG_KYRIA_OLED_CLOCK_PEEK_SECONDS 10
#define CONFIG_ZMK_IDLE_TIMEOUT 30000
#define CONFIG_SSD1306_DEFAULT_CONTRAST 128
