/*
 * SPDX-License-Identifier: MIT
 *
 * The clock face of the besim OLED screen: HH:MM in seven-segment digits and
 * a line with both battery levels. The time comes from <besim/clock.h>.
 */

#pragma once

#include <stdint.h>

#include <lvgl.h>

/*
 * Creates the face inside parent. Runs on the display work queue. Returns 0
 * or a negative error code.
 */
int oled_clock_view_init(lv_obj_t *parent);

/*
 * Redraws the face from the current time and battery levels, then moves it
 * to the place that belongs to minute_counter. Runs on the display work
 * queue.
 */
void oled_clock_view_refresh(uint32_t minute_counter);
