/*
 * SPDX-License-Identifier: MIT
 *
 * Status screen of the Kyria OLED shield: a status view and, when the clock
 * is enabled, a clock view on the same screen. One of the two is hidden.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <lvgl.h>

#include <shared/draw.h>
#include <shared/role.h>

#include "layout.h"
#include "status_view.h"
#include "keys_view.h"
#include "clock_view.h"
#include "view_manager.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(SHARED_IS_CENTRAL, "the kyria_oled shield belongs on the central half");

/* A plain container: no theme style, no padding, no scrolling. */
static lv_obj_t *make_view(lv_obj_t *screen) {
    lv_obj_t *view = lv_obj_create(screen);
    lv_obj_remove_style_all(view);
    lv_obj_remove_flag(view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(view, 0, 0);
    lv_obj_set_size(view, OLED_WIDTH, OLED_HEIGHT);
    lv_obj_set_style_bg_color(view, LVGL_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(view, LV_OPA_COVER, 0);
    return view;
}

lv_obj_t *zmk_display_status_screen() {
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, LVGL_BACKGROUND, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *status = make_view(screen);
    if (oled_status_view_init(status) != 0) {
        LOG_ERR("status view failed to start");
    }
    if (oled_keys_view_init(status) != 0) {
        LOG_ERR("keys view failed to start");
    }

#if IS_ENABLED(CONFIG_KYRIA_OLED_CLOCK)
    lv_obj_t *clock = make_view(screen);
    lv_obj_add_flag(clock, LV_OBJ_FLAG_HIDDEN);
    if (oled_clock_view_init(clock) != 0 || oled_view_manager_init(status, clock) != 0) {
        LOG_ERR("clock view failed to start");
    }
#endif

    return screen;
}
