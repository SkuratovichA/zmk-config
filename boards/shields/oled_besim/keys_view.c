/*
 * SPDX-License-Identifier: MIT
 *
 * The keys area of the besim OLED screen: one canvas per key cap, redrawn only when its state or
 * label changed, and a keyboard glyph shown while no key is on the screen.
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <lvgl.h>

#include <besim/draw.h>
#include <besim/keys_core.h>

#include "layout.h"
#include "keys_view.h"

enum cap_state {
    CAP_EMPTY,
    CAP_HELD_POP,
    CAP_HELD,
    CAP_GHOST_FRESH,
    CAP_GHOST_OLD,
};

/* What a cap canvas shows right now, to tell whether a slot needs a redraw. */
struct cap_shown {
    enum cap_state state;
    char label[KEYS_LABEL_MAX];
};

static lv_obj_t *cap_canvas[KEYS_SLOTS];
static uint8_t cap_buf[KEYS_SLOTS][BESIM_CANVAS_BUF_SIZE(OLED_CAP_CANVAS_W, OLED_CAP_CANVAS_H)]
    __aligned(4);
static struct cap_shown cap_shown[KEYS_SLOTS];

static lv_obj_t *idle_glyph;
static uint8_t idle_glyph_buf[BESIM_CANVAS_BUF_SIZE(OLED_IDLE_GLYPH_W, OLED_IDLE_GLYPH_H)]
    __aligned(4);
static bool idle_glyph_hidden;

static void draw_idle_glyph(void) {
    lv_canvas_fill_bg(idle_glyph, LVGL_BACKGROUND, LV_OPA_COVER);

    lv_draw_rect_dsc_t outline_dsc;
    init_rect_dsc(&outline_dsc, LVGL_FOREGROUND);
    outline_dsc.bg_opa = LV_OPA_TRANSP;
    outline_dsc.border_color = LVGL_FOREGROUND;
    outline_dsc.border_width = 1;
    outline_dsc.border_opa = LV_OPA_COVER;
    outline_dsc.radius = 3;
    canvas_draw_rect(idle_glyph, 0, 0, OLED_IDLE_GLYPH_W, OLED_IDLE_GLYPH_H, &outline_dsc);

    lv_draw_rect_dsc_t dot_dsc;
    init_rect_dsc(&dot_dsc, LVGL_FOREGROUND);

    for (lv_coord_t x = 4; x <= 34; x += 5) {
        canvas_draw_rect(idle_glyph, x, 4, 2, 2, &dot_dsc);
    }
    for (lv_coord_t x = 6; x <= 31; x += 5) {
        canvas_draw_rect(idle_glyph, x, 9, 2, 2, &dot_dsc);
    }
    canvas_draw_rect(idle_glyph, 4, 14, 2, 2, &dot_dsc);
    canvas_draw_rect(idle_glyph, 9, 14, 2, 2, &dot_dsc);
    canvas_draw_rect(idle_glyph, 12, 14, 14, 2, &dot_dsc);
    canvas_draw_rect(idle_glyph, 29, 14, 2, 2, &dot_dsc);
    canvas_draw_rect(idle_glyph, 34, 14, 2, 2, &dot_dsc);
}

static enum cap_state slot_state(const struct keys_slot_view *slot, int64_t now) {
    if (!slot->used) {
        return CAP_EMPTY;
    }
    if (slot->held) {
        return (now - slot->pressed_at < KEYS_POP_MS) ? CAP_HELD_POP : CAP_HELD;
    }
    return (now - slot->released_at < KEYS_GHOST_MS / 2) ? CAP_GHOST_FRESH : CAP_GHOST_OLD;
}

static void draw_slot(lv_obj_t *canvas, enum cap_state state, const char *label) {
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);

    switch (state) {
    case CAP_HELD_POP:
        draw_cap(canvas, OLED_CAP_INSET, OLED_CAP_INSET, OLED_CAP_W, OLED_CAP_H, label, true, true,
                 0);
        break;
    case CAP_HELD:
        draw_cap(canvas, OLED_CAP_INSET, OLED_CAP_INSET, OLED_CAP_W, OLED_CAP_H, label, true,
                 false, 0);
        break;
    case CAP_GHOST_FRESH:
        draw_cap(canvas, OLED_CAP_INSET, OLED_CAP_INSET, OLED_CAP_W, OLED_CAP_H, label, false,
                 false, 0);
        break;
    case CAP_GHOST_OLD:
        draw_cap(canvas, OLED_CAP_INSET, OLED_CAP_INSET, OLED_CAP_W, OLED_CAP_H, label, false,
                 false, 2);
        break;
    case CAP_EMPTY:
    default:
        break;
    }
}

/*
 * The panel is on I2C at 100 kHz, where a full frame costs about 92 ms, so only the caps whose
 * state or label changed are drawn and thereby invalidated.
 */
static void keys_redraw(void) {
    int64_t now = k_uptime_get();
    struct keys_slot_view view[KEYS_SLOTS];
    bool any = keys_core_snapshot(view, now);

    for (int i = 0; i < KEYS_SLOTS; i++) {
        struct cap_shown next;
        memset(&next, 0, sizeof(next));
        next.state = slot_state(&view[i], now);
        if (next.state != CAP_EMPTY) {
            strncpy(next.label, view[i].label, KEYS_LABEL_MAX - 1);
        }

        if (next.state == cap_shown[i].state && strcmp(next.label, cap_shown[i].label) == 0) {
            continue;
        }
        cap_shown[i] = next;
        draw_slot(cap_canvas[i], next.state, next.label);
    }

    bool hide_glyph = any;
    if (hide_glyph != idle_glyph_hidden) {
        idle_glyph_hidden = hide_glyph;
        if (hide_glyph) {
            lv_obj_add_flag(idle_glyph, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_remove_flag(idle_glyph, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void keys_timer_cb(lv_timer_t *timer) { keys_redraw(); }

static void keys_redraw_work_cb(struct k_work *work) { keys_redraw(); }

K_WORK_DEFINE(keys_redraw_work, keys_redraw_work_cb);

int oled_keys_view_init(lv_obj_t *parent) {
    for (int i = 0; i < KEYS_SLOTS; i++) {
        cap_canvas[i] = lv_canvas_create(parent);
        if (cap_canvas[i] == NULL) {
            return -ENOMEM;
        }
        lv_canvas_set_buffer(cap_canvas[i], cap_buf[i], OLED_CAP_CANVAS_W, OLED_CAP_CANVAS_H,
                             BESIM_CANVAS_COLOR_FORMAT);
        lv_obj_set_pos(cap_canvas[i], (i % OLED_KEYS_COLS) * OLED_CAP_PITCH_X,
                       OLED_KEYS_Y + (i / OLED_KEYS_COLS) * OLED_CAP_PITCH_Y);
        lv_canvas_fill_bg(cap_canvas[i], LVGL_BACKGROUND, LV_OPA_COVER);
        cap_shown[i].state = CAP_EMPTY;
        cap_shown[i].label[0] = '\0';
    }

    idle_glyph = lv_canvas_create(parent);
    if (idle_glyph == NULL) {
        return -ENOMEM;
    }
    lv_canvas_set_buffer(idle_glyph, idle_glyph_buf, OLED_IDLE_GLYPH_W, OLED_IDLE_GLYPH_H,
                         BESIM_CANVAS_COLOR_FORMAT);
    lv_obj_set_pos(idle_glyph, OLED_IDLE_GLYPH_X, OLED_IDLE_GLYPH_Y);
    draw_idle_glyph();
    idle_glyph_hidden = false;

    if (lv_timer_create(keys_timer_cb, KEYS_TICK_MS, NULL) == NULL) {
        return -ENOMEM;
    }
    keys_core_set_redraw_work(&keys_redraw_work);

    return 0;
}
