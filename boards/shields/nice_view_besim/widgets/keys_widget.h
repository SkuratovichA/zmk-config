/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>

/*
 * Pressed-keys view for the middle band: every key currently held is drawn as a
 * keycap with its label (resolved from the active layer of the keymap); a
 * released key lingers as a hollow cap for a moment. Placed at landscape offset
 * `x` inside `parent`.
 */
int zmk_widget_keys_init(lv_obj_t *parent, lv_coord_t x);
