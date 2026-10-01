/*
 * SPDX-License-Identifier: MIT
 *
 * Drawing helpers shared by the shared status screens: colours, canvas
 * buffers, and the battery row and key cap that both screens show.
 * Based on ZMK's stock nice_view widgets (LVGL 9).
 */

#pragma once

#include <lvgl.h>
#include <zephyr/sys/util.h>

/* L8 is the smallest canvas format that sw_rotate supports on the nice!view. */
#define SHARED_CANVAS_COLOR_FORMAT LV_COLOR_FORMAT_L8
#define SHARED_CANVAS_BUF_SIZE(w, h)                                                               \
    LV_CANVAS_BUF_SIZE((w), (h), LV_COLOR_FORMAT_GET_BPP(SHARED_CANVAS_COLOR_FORMAT),              \
                       LV_DRAW_BUF_STRIDE_ALIGN)

#define LVGL_BACKGROUND                                                                            \
    (IS_ENABLED(CONFIG_SHARED_DISPLAY_INVERTED) ? lv_color_black() : lv_color_white())
#define LVGL_FOREGROUND                                                                            \
    (IS_ENABLED(CONFIG_SHARED_DISPLAY_INVERTED) ? lv_color_white() : lv_color_black())

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
                    lv_text_align_t align);
void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color);
void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color, uint8_t width);
void init_arc_dsc(lv_draw_arc_dsc_t *arc_dsc, lv_color_t color, uint8_t width);

void canvas_draw_line(lv_obj_t *canvas, const lv_point_t points[], uint32_t point_cnt,
                      lv_draw_line_dsc_t *draw_dsc);
/* The rectangle covers x to x + w - 1 and y to y + h - 1. */
void canvas_draw_rect(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                      lv_draw_rect_dsc_t *draw_dsc);
void canvas_draw_arc(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t r,
                     int32_t start_angle, int32_t end_angle, lv_draw_arc_dsc_t *draw_dsc);
/* The text area spans x to x + max_w and reaches down to the canvas height. */
void canvas_draw_text(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t max_w,
                      lv_draw_label_dsc_t *draw_dsc, const char *txt);
void canvas_draw_img(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, const lv_image_dsc_t *src,
                     lv_draw_image_dsc_t *draw_dsc);

/*
 * One battery cell: label, body of body_w by 10, nub, and the level as text.
 * The cell starts at x and is body_w + 44 wide. An unknown level shows "--".
 */
void draw_battery_row(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t body_w,
                      const char *label, uint8_t level, bool charging, bool known);

/*
 * One key cap of w by h at x, y. A solid cap is a held key, a hollow one a
 * ghost. pop adds an outline two pixels outside the cap, so the canvas needs
 * that margin around it. shrink insets the cap by that many pixels. Labels of
 * up to three characters use Montserrat 12, longer ones UNSCII 8.
 */
void draw_cap(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
              const char *label, bool solid, bool pop, lv_coord_t shrink);
