/*
 * SPDX-License-Identifier: MIT
 *
 * Decides which view the Kyria OLED screen shows.
 *
 *   status      the normal screen
 *   idle clock  the clock at low contrast, after
 *               CONFIG_KYRIA_OLED_CLOCK_IDLE_SECONDS without a key press,
 *               until the next key press
 *   peek        the clock at normal contrast, for
 *               CONFIG_KYRIA_OLED_CLOCK_PEEK_SECONDS after a clock key, or
 *               until any other key is pressed
 */

#pragma once

#include <lvgl.h>

/*
 * Takes the two containers, the clock one hidden, and starts the listeners.
 * Runs on the display work queue. Returns 0 or a negative error code.
 */
int oled_view_manager_init(lv_obj_t *status_container, lv_obj_t *clock_container);
