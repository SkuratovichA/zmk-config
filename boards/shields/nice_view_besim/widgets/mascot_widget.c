/*
 * SPDX-License-Identifier: MIT
 *
 * Animated 1-bit mascot for the nice!view. Frames live in mascot.c (generated).
 *
 *  - every key press flips between two "typing" frames (sparks!)
 *  - ~350 ms after the last press it goes back to idle and blinks now and then
 *  - when ZMK reports the keyboard idle, it sleeps (and the timer slows down)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/activity.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/position_state_changed.h>

#include "mascot_widget.h"

LV_IMG_DECLARE(mascot_idle);
LV_IMG_DECLARE(mascot_blink);
LV_IMG_DECLARE(mascot_type_a);
LV_IMG_DECLARE(mascot_type_b);
LV_IMG_DECLARE(mascot_sleep);

#define MASCOT_TICK_MS 100
#define MASCOT_SLEEP_TICK_MS 1000
#define MASCOT_TYPING_HOLD_MS 350
#define MASCOT_BLINK_EVERY_TICKS 38 /* ~3.8 s between blinks while idle */

/* All of this state is only touched from the display work queue (LVGL's thread). */
static lv_obj_t *mascot_img;
static lv_timer_t *mascot_timer;
static const lv_image_dsc_t *mascot_current;
static int64_t mascot_last_press;
static bool mascot_alternate;
static bool mascot_sleeping;
static uint32_t mascot_ticks;

static void mascot_show(const lv_image_dsc_t *src) {
    if (mascot_img == NULL || src == mascot_current) {
        return;
    }
    mascot_current = src;
    lv_image_set_src(mascot_img, src);
}

static void mascot_refresh(void) {
    if (mascot_sleeping) {
        mascot_show(&mascot_sleep);
        return;
    }

    const int64_t since_press = k_uptime_get() - mascot_last_press;
    if (since_press >= 0 && since_press < MASCOT_TYPING_HOLD_MS) {
        mascot_show(mascot_alternate ? &mascot_type_a : &mascot_type_b);
    } else if (mascot_ticks % MASCOT_BLINK_EVERY_TICKS == 0) {
        mascot_show(&mascot_blink);
    } else {
        mascot_show(&mascot_idle);
    }
}

static void mascot_tick_cb(lv_timer_t *timer) {
    mascot_ticks++;
    mascot_refresh();
}

/* Key presses ------------------------------------------------------------ */

struct mascot_press_state {
    bool pressed;
    int64_t timestamp;
};

static struct mascot_press_state mascot_press_get_state(const zmk_event_t *eh) {
    if (eh == NULL) {
        return (struct mascot_press_state){.pressed = false, .timestamp = 0};
    }
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL) {
        return (struct mascot_press_state){.pressed = false, .timestamp = 0};
    }
    return (struct mascot_press_state){.pressed = ev->state, .timestamp = k_uptime_get()};
}

static void mascot_press_update_cb(struct mascot_press_state state) {
    if (!state.pressed) {
        return;
    }
    mascot_last_press = state.timestamp;
    mascot_alternate = !mascot_alternate;
    mascot_refresh();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_mascot_press, struct mascot_press_state,
                            mascot_press_update_cb, mascot_press_get_state)
ZMK_SUBSCRIPTION(widget_mascot_press, zmk_position_state_changed);

/* Activity (idle / sleep) ------------------------------------------------ */

struct mascot_activity_state {
    bool sleeping;
};

static struct mascot_activity_state mascot_activity_get_state(const zmk_event_t *eh) {
    return (struct mascot_activity_state){.sleeping =
                                              zmk_activity_get_state() != ZMK_ACTIVITY_ACTIVE};
}

static void mascot_activity_update_cb(struct mascot_activity_state state) {
    mascot_sleeping = state.sleeping;
    if (mascot_timer != NULL) {
        lv_timer_set_period(mascot_timer, state.sleeping ? MASCOT_SLEEP_TICK_MS : MASCOT_TICK_MS);
    }
    mascot_refresh();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_mascot_activity, struct mascot_activity_state,
                            mascot_activity_update_cb, mascot_activity_get_state)
ZMK_SUBSCRIPTION(widget_mascot_activity, zmk_activity_state_changed);

/* ------------------------------------------------------------------------ */

int zmk_widget_mascot_init(lv_obj_t *parent, lv_coord_t x) {
    mascot_img = lv_image_create(parent);
    lv_obj_align(mascot_img, LV_ALIGN_TOP_LEFT, x, 0);

    mascot_last_press = k_uptime_get() - MASCOT_TYPING_HOLD_MS;
    mascot_ticks = 1;
    mascot_show(&mascot_idle);

    mascot_timer = lv_timer_create(mascot_tick_cb, MASCOT_TICK_MS, NULL);

    widget_mascot_press_init();
    widget_mascot_activity_init();
    return 0;
}
