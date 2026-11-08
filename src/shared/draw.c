/*
 * SPDX-License-Identifier: MIT
 *
 * Drawing helpers shared by the shared status screens: canvas wrappers, the battery row and the
 * key cap. Based on ZMK's stock nice_view widgets (LVGL 9).
 */

#include <stdio.h>
#include <string.h>

#include <shared/draw.h>

LV_IMG_DECLARE(charge_glyph);

void draw_battery_row(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t body_w,
                      const char *label, uint8_t level, bool charging, bool known) {
    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
    lv_draw_label_dsc_t label_left_dsc;
    init_label_dsc(&label_left_dsc, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_LEFT);
    lv_draw_label_dsc_t label_right_dsc;
    init_label_dsc(&label_right_dsc, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_RIGHT);

    // "L" / "R"
    canvas_draw_text(canvas, x, y + 1, 9, &label_left_dsc, label);

    // Battery body (body_w x 10) with a 2x4 nub; the fill is 0..body_w - 4 px wide.
    canvas_draw_rect(canvas, x + 10, y, body_w, 10, &rect_white_dsc);
    canvas_draw_rect(canvas, x + 11, y + 1, body_w - 2, 8, &rect_black_dsc);
    if (known) {
        const lv_coord_t fill = (level * (body_w - 4) + 50) / 100;
        if (fill > 0) {
            canvas_draw_rect(canvas, x + 12, y + 2, fill, 6, &rect_white_dsc);
        }
    }
    canvas_draw_rect(canvas, x + 10 + body_w, y + 3, 2, 4, &rect_white_dsc);

    if (charging) {
        lv_draw_image_dsc_t img_dsc;
        lv_draw_image_dsc_init(&img_dsc);
        canvas_draw_img(canvas, x + 10 + (body_w - 8) / 2, y, &charge_glyph, &img_dsc);
    }

    char text[6];
    if (known) {
        snprintf(text, sizeof(text), "%u%%", level);
    } else {
        strncpy(text, "--", sizeof(text));
    }
    canvas_draw_text(canvas, x + 12 + body_w, y + 1, 32, &label_right_dsc, text);
}

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
                    lv_text_align_t align) {
    lv_draw_label_dsc_init(label_dsc);
    label_dsc->color = color;
    label_dsc->font = font;
    label_dsc->align = align;
}

void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color) {
    lv_draw_rect_dsc_init(rect_dsc);
    rect_dsc->bg_color = bg_color;
}

void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color, uint8_t width) {
    lv_draw_line_dsc_init(line_dsc);
    line_dsc->color = color;
    line_dsc->width = width;
}

void init_arc_dsc(lv_draw_arc_dsc_t *arc_dsc, lv_color_t color, uint8_t width) {
    lv_draw_arc_dsc_init(arc_dsc);
    arc_dsc->color = color;
    arc_dsc->width = width;
}

void canvas_draw_line(lv_obj_t *canvas, const lv_point_t points[], uint32_t point_cnt,
                      lv_draw_line_dsc_t *draw_dsc) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    for (uint32_t i = 1; i < point_cnt; ++i) {
        draw_dsc->p1.x = points[i - 1].x;
        draw_dsc->p1.y = points[i - 1].y;
        draw_dsc->p2.x = points[i].x;
        draw_dsc->p2.y = points[i].y;
        lv_draw_line(&layer, draw_dsc);
    }

    lv_canvas_finish_layer(canvas, &layer);
}

void canvas_draw_rect(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                      lv_draw_rect_dsc_t *draw_dsc) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    lv_area_t coords = {x, y, x + w - 1, y + h - 1};
    lv_draw_rect(&layer, draw_dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);
}

void canvas_draw_arc(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t r,
                     int32_t start_angle, int32_t end_angle, lv_draw_arc_dsc_t *draw_dsc) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    draw_dsc->center.x = x;
    draw_dsc->center.y = y;
    draw_dsc->radius = r;
    draw_dsc->start_angle = start_angle;
    draw_dsc->end_angle = end_angle;
    lv_draw_arc(&layer, draw_dsc);

    lv_canvas_finish_layer(canvas, &layer);
}

void canvas_draw_text(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t max_w,
                      lv_draw_label_dsc_t *draw_dsc, const char *txt) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    draw_dsc->text = txt;
    lv_area_t coords = {x, y, x + max_w, y + lv_canvas_get_draw_buf(canvas)->header.h};
    lv_draw_label(&layer, draw_dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);
}

void canvas_draw_img(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, const lv_image_dsc_t *src,
                     lv_draw_image_dsc_t *draw_dsc) {
    lv_layer_t layer;
    lv_canvas_init_layer(canvas, &layer);

    draw_dsc->src = src;
    lv_area_t coords = {x, y, x + src->header.w - 1, y + src->header.h - 1};
    lv_draw_image(&layer, draw_dsc, &coords);

    lv_canvas_finish_layer(canvas, &layer);
}

void draw_cap(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
              const char *label, bool solid, bool pop, lv_coord_t shrink) {
    lv_draw_rect_dsc_t fg_dsc;
    init_rect_dsc(&fg_dsc, LVGL_FOREGROUND);
    fg_dsc.radius = 4;
    lv_draw_rect_dsc_t bg_dsc;
    init_rect_dsc(&bg_dsc, LVGL_BACKGROUND);
    bg_dsc.radius = 3;

    x += shrink;
    y += shrink;
    w -= 2 * shrink;
    h -= 2 * shrink;

    if (pop) {
        canvas_draw_rect(canvas, x - 2, y - 2, w + 4, h + 4, &fg_dsc);
        canvas_draw_rect(canvas, x - 1, y - 1, w + 2, h + 2, &bg_dsc);
    }
    canvas_draw_rect(canvas, x, y, w, h, &fg_dsc);
    if (!solid) {
        canvas_draw_rect(canvas, x + 1, y + 1, w - 2, h - 2, &bg_dsc);
    }

    const bool wide = strlen(label) > 3;
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, solid ? LVGL_BACKGROUND : LVGL_FOREGROUND,
                   wide ? &lv_font_unscii_8 : &lv_font_montserrat_12, LV_TEXT_ALIGN_CENTER);
    /* A five-character label (40 px of UNSCII 8) overflows the cap by a pixel or two;
     * without this flag LVGL would wrap its last character onto a second line instead. */
    label_dsc.flag |= LV_TEXT_FLAG_EXPAND;
    const lv_coord_t text_y = y + (wide ? (h - 8) / 2 : (h - 14) / 2);
    canvas_draw_text(canvas, x, text_y, w, &label_dsc, label);
}
