/*
 * SPDX-License-Identifier: MIT
 *
 * Switches the Kyria OLED screen between the status view and the clock: an
 * idle clock after a period without key presses, and a peek after a clock key.
 * All view changes run on the display work queue.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/util.h>

#include <lvgl.h>

#include <zmk/activity.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#include <besim/clock.h>
#include <besim/keys_core.h>

#include "clock_view.h"
#include "view_manager.h"

#ifdef CONFIG_SSD1306_DEFAULT_CONTRAST
#define NORMAL_CONTRAST CONFIG_SSD1306_DEFAULT_CONTRAST
#else
#define NORMAL_CONTRAST 0xFF
#endif

enum view_mode {
    VIEW_STATUS,
    VIEW_IDLE_CLOCK,
    VIEW_PEEK,
};

static enum view_mode mode;
static lv_obj_t *status_view;
static lv_obj_t *clock_view;
static uint32_t minute_counter;
static const struct device *display __maybe_unused = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

static void idle_enter_handler(struct k_work *work);
static void leave_handler(struct k_work *work);
static void peek_start_handler(struct k_work *work);
static void peek_end_handler(struct k_work *work);
static void peek_cancel_handler(struct k_work *work);
static void minute_handler(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(idle_enter_work, idle_enter_handler);
static K_WORK_DEFINE(leave_work, leave_handler);
static K_WORK_DEFINE(peek_start_work, peek_start_handler);
static K_WORK_DELAYABLE_DEFINE(peek_end_work, peek_end_handler);
static K_WORK_DEFINE(peek_cancel_work, peek_cancel_handler);
static K_WORK_DELAYABLE_DEFINE(minute_work, minute_handler);

static void show_clock(void) {
    lv_obj_add_flag(status_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(clock_view, LV_OBJ_FLAG_HIDDEN);
    oled_clock_view_refresh(minute_counter);
}

static void show_status(void) {
    lv_obj_add_flag(clock_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(status_view, LV_OBJ_FLAG_HIDDEN);
}

/* True once oled_view_manager_init() ran; the work items do nothing before. */
static bool views_ready(void) { return status_view != NULL && clock_view != NULL; }

static void set_contrast(uint8_t value) {
#ifdef CONFIG_SSD1306_DEFAULT_CONTRAST
    display_set_contrast(display, value);
#else
    ARG_UNUSED(value);
#endif
}

static void schedule_minute_work(void) {
    k_work_reschedule_for_queue(zmk_display_work_q(), &minute_work,
                                K_MSEC(besim_clock_ms_to_next_minute()));
}

static void idle_enter_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!views_ready()) {
        return;
    }

    if (mode == VIEW_IDLE_CLOCK) {
        return;
    }
    mode = VIEW_IDLE_CLOCK;
    k_work_cancel_delayable(&peek_end_work);
    show_clock();
    set_contrast(CONFIG_KYRIA_OLED_CLOCK_CONTRAST);
    schedule_minute_work();
}

/*
 * In VIEW_PEEK this does nothing: a clock key pressed while the idle clock
 * shows must end in a peek whatever the order in which this work and the peek
 * start reach the queue.
 */
static void leave_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!views_ready()) {
        return;
    }

    if (mode != VIEW_IDLE_CLOCK) {
        return;
    }
    mode = VIEW_STATUS;
    show_status();
    set_contrast(NORMAL_CONTRAST);
    k_work_cancel_delayable(&minute_work);
}

static void peek_start_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!views_ready()) {
        return;
    }

    mode = VIEW_PEEK;
    show_clock();
    set_contrast(NORMAL_CONTRAST);
    k_work_reschedule_for_queue(zmk_display_work_q(), &peek_end_work,
                                K_MSEC(CONFIG_KYRIA_OLED_CLOCK_PEEK_SECONDS * 1000));
    schedule_minute_work();
}

static void end_peek(void) {
    if (mode != VIEW_PEEK) {
        return;
    }
    mode = VIEW_STATUS;
    show_status();
    k_work_cancel_delayable(&minute_work);
}

static void peek_end_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!views_ready()) {
        return;
    }
    end_peek();
}

static void peek_cancel_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!views_ready()) {
        return;
    }
    end_peek();
}

static void minute_handler(struct k_work *work) {
    ARG_UNUSED(work);
    if (!views_ready()) {
        return;
    }

    if (mode == VIEW_STATUS) {
        return;
    }
    minute_counter++;
    oled_clock_view_refresh(minute_counter);
    schedule_minute_work();
}

static int activity_cb(const zmk_event_t *eh) {
    struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    switch (ev->state) {
    case ZMK_ACTIVITY_IDLE: {
        /* ZMK raises IDLE after CONFIG_ZMK_IDLE_TIMEOUT ms without activity. */
        int64_t delay =
            MAX(0, (int64_t)CONFIG_KYRIA_OLED_CLOCK_IDLE_SECONDS * 1000 - CONFIG_ZMK_IDLE_TIMEOUT);
        k_work_schedule_for_queue(zmk_display_work_q(), &idle_enter_work, K_MSEC(delay));
        break;
    }
    case ZMK_ACTIVITY_ACTIVE:
        k_work_cancel_delayable(&idle_enter_work);
        k_work_submit_to_queue(zmk_display_work_q(), &leave_work);
        break;
    case ZMK_ACTIVITY_SLEEP:
        /* The driver suspend cuts the power rail of the panel. */
        break;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(oled_view_manager, activity_cb);
ZMK_SUBSCRIPTION(oled_view_manager, zmk_activity_state_changed);

static void clock_listener(enum besim_clock_event event) {
    switch (event) {
    case BESIM_CLOCK_EVENT_SHOW:
    case BESIM_CLOCK_EVENT_ADJUSTED:
        /* A second key extends the peek: the start work reschedules its end. */
        k_work_submit_to_queue(zmk_display_work_q(), &peek_start_work);
        break;
    }
}

/*
 * A clock key reaches the behaviour, whose listener starts the peek, so the
 * order of the ZMK listeners does not matter here.
 */
static void press_cb(bool clock_key) {
    if (!clock_key) {
        k_work_submit_to_queue(zmk_display_work_q(), &peek_cancel_work);
    }
}

int oled_view_manager_init(lv_obj_t *status_container, lv_obj_t *clock_container) {
    status_view = status_container;
    clock_view = clock_container;
    mode = VIEW_STATUS;
    minute_counter = 0;

    besim_clock_set_listener(clock_listener);
    keys_core_set_press_callback(press_cb);
    return 0;
}
