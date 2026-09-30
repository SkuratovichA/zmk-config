/*
 * SPDX-License-Identifier: MIT
 *
 * The status strip of the besim OLED screen: both batteries, the output with
 * the Bluetooth profiles, and the layer name.
 */

#pragma once

#include <lvgl.h>

/*
 * Creates the strip at the top of parent and starts its listeners. Runs on
 * the display work queue. Returns 0 or a negative error code.
 */
int oled_status_view_init(lv_obj_t *parent);
