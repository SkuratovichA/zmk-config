/*
 * SPDX-License-Identifier: MIT
 *
 * Pressed-keys view: a 2x3 grid of keycaps in the middle band of the screen.
 *
 *  - a key that is held is a solid cap with its label knocked out
 *  - for the first ~120 ms after the press the cap "pops" (extra outline)
 *  - a released key stays as a hollow cap for ~600 ms, shrinking half-way, then goes
 *  - with nothing held the band shows a small keyboard glyph
 *
 * The keys, their labels and the slots come from besim/keys_core.h; this file only draws them.
 */

#include <zephyr/kernel.h>

#include <shared/keys_core.h>

#include "keys_widget.h"
#include "util.h"

#define CAP_W 33
#define CAP_H 20
#define CAP_GAP_X 2
#define CAP_GAP_Y 2
#define CAP_TOP 2

/* Display-thread only. */
static lv_obj_t *keys_canvas;
static uint8_t keys_cbuf[CANVAS_BUF_SIZE];
static lv_timer_t *keys_timer;
static bool keys_idle_drawn;

/* Drawing (display thread) -------------------------------------------- */

static void draw_idle(lv_obj_t *canvas) {
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_CENTER);
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    canvas_draw_text(canvas, 0, 26, CANVAS_SIZE, &label_dsc, LV_SYMBOL_KEYBOARD);
    rotate_canvas(canvas);
}

static void keys_redraw(void) {
    if (keys_canvas == NULL) {
        return;
    }

    const int64_t now = k_uptime_get();
    struct keys_slot_view view[KEYS_SLOTS];
    const bool any = keys_core_snapshot(view, now);

    if (!any) {
        if (!keys_idle_drawn) {
            draw_idle(keys_canvas);
            keys_idle_drawn = true;
        }
        return;
    }
    keys_idle_drawn = false;

    lv_canvas_fill_bg(keys_canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    for (int i = 0; i < KEYS_SLOTS; i++) {
        if (!view[i].used) {
            continue;
        }
        const lv_coord_t x = (i % 2) * (CAP_W + CAP_GAP_X);
        const lv_coord_t y = CAP_TOP + (i / 2) * (CAP_H + CAP_GAP_Y);
        if (view[i].held) {
            const bool pop = now - view[i].pressed_at < KEYS_POP_MS;
            draw_cap(keys_canvas, x, y, CAP_W, CAP_H, view[i].label, true, pop, 0);
        } else {
            const lv_coord_t shrink = (now - view[i].released_at < KEYS_GHOST_MS / 2) ? 0 : 2;
            draw_cap(keys_canvas, x, y, CAP_W, CAP_H, view[i].label, false, false, shrink);
        }
    }
    rotate_canvas(keys_canvas);
}

static void keys_redraw_work_cb(struct k_work *work) { keys_redraw(); }
K_WORK_DEFINE(keys_redraw_work, keys_redraw_work_cb);

static void keys_tick_cb(lv_timer_t *timer) { keys_redraw(); }

/* ---------------------------------------------------------------------- */

int zmk_widget_keys_init(lv_obj_t *parent, lv_coord_t x) {
    keys_canvas = lv_canvas_create(parent);
    lv_obj_align(keys_canvas, LV_ALIGN_TOP_LEFT, x, 0);
    lv_canvas_set_buffer(keys_canvas, keys_cbuf, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    keys_core_set_redraw_work(&keys_redraw_work);
    keys_redraw();
    keys_timer = lv_timer_create(keys_tick_cb, KEYS_TICK_MS, NULL);
    return 0;
}
