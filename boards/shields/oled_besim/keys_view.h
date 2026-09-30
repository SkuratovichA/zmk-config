/*
 * SPDX-License-Identifier: MIT
 *
 * The keys area of the besim OLED screen: the keys being pressed as caps, and
 * a keyboard glyph while there are none. The keys themselves come from
 * <besim/keys_core.h>.
 */

#pragma once

#include <lvgl.h>

/*
 * Creates the cap canvases and the idle glyph inside parent, and starts the
 * redraw timer. Runs on the display work queue. Returns 0 or a negative error
 * code.
 */
int oled_keys_view_init(lv_obj_t *parent);
