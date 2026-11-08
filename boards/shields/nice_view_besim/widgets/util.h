/*
 * SPDX-License-Identifier: MIT
 *
 * Shared drawing helpers for the besimboard nice!view status screen.
 * Based on ZMK's stock nice_view widgets (LVGL 9).
 */

#pragma once

#include <lvgl.h>
#include <zmk/endpoints.h>

#include <shared/draw.h>
#include <shared/role.h>
#include <shared/status_state.h>

#define NICEVIEW_PROFILE_COUNT SHARED_PROFILE_COUNT
#define NICEVIEW_IS_CENTRAL SHARED_IS_CENTRAL

#define CANVAS_SIZE 68
#define CANVAS_COLOR_FORMAT SHARED_CANVAS_COLOR_FORMAT // smallest type supported by sw_rotate
#define CANVAS_BUF_SIZE SHARED_CANVAS_BUF_SIZE(CANVAS_SIZE, CANVAS_SIZE)

/*
 * The nice!view is a 160x68 panel mounted vertically: memory x=160 is the physical
 * top. Each 68x68 block is drawn upright and then rotated into place, so the
 * screen is three vertical bands, addressed by their landscape x offset.
 */
#define BAND_TOP_X 92     /* physical top 68 px: batteries, output, profiles */
#define BAND_MIDDLE_X 24  /* next 68 px: the mascot */
#define BAND_BOTTOM_X -44 /* last 24 px: layer name (only the canvas' first 24 rows show) */

struct status_state {
    uint8_t battery;
    bool charging;
#if NICEVIEW_IS_CENTRAL
    bool peripheral_known;
    uint8_t peripheral_battery;
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool profiles_connected[NICEVIEW_PROFILE_COUNT];
    bool profiles_bonded[NICEVIEW_PROFILE_COUNT];
    uint8_t layer_index;
    const char *layer_label;
#else
    bool connected;
#endif
};

void rotate_canvas(lv_obj_t *canvas);
