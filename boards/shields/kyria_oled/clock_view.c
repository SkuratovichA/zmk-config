/*
 * SPDX-License-Identifier: MIT
 *
 * The clock face of the Kyria OLED screen: HH:MM in seven-segment digits drawn
 * from filled rectangles, and a line with both battery levels.
 */

#include <errno.h>
#include <stdio.h>

#include <lvgl.h>

#include <besim/clock.h>
#include <besim/draw.h>
#include <besim/status_state.h>

#include "layout.h"
#include "clock_view.h"

#define SEG_A BIT(0)
#define SEG_B BIT(1)
#define SEG_C BIT(2)
#define SEG_D BIT(3)
#define SEG_E BIT(4)
#define SEG_F BIT(5)
#define SEG_G BIT(6)

/* Segments lit for each digit 0..9. */
static const uint8_t digit_segments[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,         /* 0 */
    SEG_B | SEG_C,                                         /* 1 */
    SEG_A | SEG_B | SEG_G | SEG_E | SEG_D,                 /* 2 */
    SEG_A | SEG_B | SEG_G | SEG_C | SEG_D,                 /* 3 */
    SEG_F | SEG_G | SEG_B | SEG_C,                         /* 4 */
    SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,                 /* 5 */
    SEG_A | SEG_F | SEG_G | SEG_E | SEG_D | SEG_C,         /* 6 */
    SEG_A | SEG_B | SEG_C,                                 /* 7 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G, /* 8 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,         /* 9 */
};

static const lv_coord_t digit_x[4] = {OLED_CLOCK_DIGIT_X0, OLED_CLOCK_DIGIT_X1,
                                      OLED_CLOCK_DIGIT_X2, OLED_CLOCK_DIGIT_X3};

static uint8_t canvas_buf[BESIM_CANVAS_BUF_SIZE(OLED_CLOCK_FACE_W, OLED_CLOCK_FACE_H)]
    __aligned(LV_DRAW_BUF_ALIGN);
static lv_obj_t *canvas;

static void draw_digit(lv_coord_t x, uint8_t segments, lv_draw_rect_dsc_t *rect_dsc) {
    const lv_coord_t t = OLED_CLOCK_SEGMENT;

    const lv_coord_t w = OLED_CLOCK_DIGIT_W;
    const lv_coord_t half = OLED_CLOCK_DIGIT_H / 2 + t / 2; /* 20: down to the middle bar */
    const lv_coord_t mid = OLED_CLOCK_DIGIT_H / 2 - t / 2;  /* 16: top of the middle bar */

    /* The bars overlap at the corners, so a digit reads as one solid shape. */
    if (segments & SEG_A) {
        canvas_draw_rect(canvas, x, 0, w, t, rect_dsc);
    }
    if (segments & SEG_G) {
        canvas_draw_rect(canvas, x, mid, w, t, rect_dsc);
    }
    if (segments & SEG_D) {
        canvas_draw_rect(canvas, x, OLED_CLOCK_DIGIT_H - t, w, t, rect_dsc);
    }
    if (segments & SEG_F) {
        canvas_draw_rect(canvas, x, 0, t, half, rect_dsc);
    }
    if (segments & SEG_B) {
        canvas_draw_rect(canvas, x + w - t, 0, t, half, rect_dsc);
    }
    if (segments & SEG_E) {
        canvas_draw_rect(canvas, x, mid, t, half, rect_dsc);
    }
    if (segments & SEG_C) {
        canvas_draw_rect(canvas, x + w - t, mid, t, half, rect_dsc);
    }
}

static void draw_battery_line(void) {
    struct besim_battery_status_state left = besim_battery_status_get_state(NULL);
    struct besim_peripheral_battery_status_state right =
        besim_peripheral_battery_status_get_state(NULL);
    char buf[16];

    if (right.known) {
        snprintf(buf, sizeof buf, "L%3u%% R%3u%%", (unsigned int)left.level,
                 (unsigned int)right.level);
    } else {
        snprintf(buf, sizeof buf, "L%3u%% R --%%", (unsigned int)left.level);
    }

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_LEFT);
    canvas_draw_text(canvas, OLED_CLOCK_BATTERY_X, OLED_CLOCK_BATTERY_Y, 88, &label_dsc, buf);
}

int oled_clock_view_init(lv_obj_t *parent) {
    canvas = lv_canvas_create(parent);
    if (canvas == NULL) {
        return -ENOMEM;
    }

    lv_obj_set_pos(canvas, 0, 0);
    lv_canvas_set_buffer(canvas, canvas_buf, OLED_CLOCK_FACE_W, OLED_CLOCK_FACE_H,
                         BESIM_CANVAS_COLOR_FORMAT);
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
    return 0;
}

void oled_clock_view_refresh(uint32_t minute_counter) {
    struct besim_clock_time t;
    uint8_t segments[4];

    besim_clock_now(&t);
    if (t.set) {
        segments[0] = digit_segments[t.hour / 10];
        segments[1] = digit_segments[t.hour % 10];
        segments[2] = digit_segments[t.minute / 10];
        segments[3] = digit_segments[t.minute % 10];
    } else {
        for (int i = 0; i < 4; i++) {
            segments[i] = SEG_G;
        }
    }

    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);

    lv_draw_rect_dsc_t rect_dsc;
    init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);

    for (int i = 0; i < 4; i++) {
        draw_digit(digit_x[i], segments[i], &rect_dsc);
    }
    canvas_draw_rect(canvas, OLED_CLOCK_COLON_X, OLED_CLOCK_COLON_Y_TOP, OLED_CLOCK_COLON_SIZE,
                     OLED_CLOCK_COLON_SIZE, &rect_dsc);
    canvas_draw_rect(canvas, OLED_CLOCK_COLON_X, OLED_CLOCK_COLON_Y_BOTTOM, OLED_CLOCK_COLON_SIZE,
                     OLED_CLOCK_COLON_SIZE, &rect_dsc);

    draw_battery_line();

    lv_obj_set_pos(canvas, (minute_counter * 7) % OLED_CLOCK_SHIFT_X_RANGE,
                   (minute_counter * 5) % OLED_CLOCK_SHIFT_Y_RANGE);
}
