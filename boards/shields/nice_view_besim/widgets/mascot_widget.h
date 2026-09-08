/*
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <lvgl.h>

/*
 * The animated mascot: a 68x68 image placed at landscape offset `x` inside
 * `parent`. It reacts to key presses on this half (and, on the central, to
 * the peripheral's keys too), blinks while idle and sleeps when ZMK goes idle.
 */
int zmk_widget_mascot_init(lv_obj_t *parent, lv_coord_t x);
