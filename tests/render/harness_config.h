/*
 * SPDX-License-Identifier: MIT
 *
 * The Kconfig symbols of the besimboard left half (the central), force-included with -include
 * into the widget sources, the stubs and main.c. Not applied to LVGL, which is configured by
 * autoconf_lv.h and lv_conf_host.h.
 *
 * CONFIG_NICE_VIEW_BESIM_WIDGET_INVERTED is left undefined on purpose: the Lily draws black on
 * white. The shared sources that a later stage wires in for the new Lily additionally need
 * CONFIG_SHARED_DISPLAY_INVERTED to stay undefined, so do not define it here.
 */

#pragma once

#define CONFIG_ZMK_SPLIT 1
#define CONFIG_ZMK_SPLIT_ROLE_CENTRAL 1
#define CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING 1
#define CONFIG_ZMK_BLE 1
#define CONFIG_USB_DEVICE_STACK 1
#define CONFIG_ZMK_KEYBOARD_NAME "besimboard"
#define CONFIG_ZMK_LOG_LEVEL 0
#define CONFIG_BT_MAX_PAIRED 5
