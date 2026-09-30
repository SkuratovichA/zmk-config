/*
 * SPDX-License-Identifier: MIT
 *
 * A wall clock without a time source of its own: the time is set by hand and
 * counted from the uptime. It is lost on every reset, and waking from deep
 * sleep is a reset.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

struct besim_clock_time {
    bool set;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

enum besim_clock_event {
    /* A key asked for the clock to be shown. */
    BESIM_CLOCK_EVENT_SHOW,
    /* The time was changed by hand. */
    BESIM_CLOCK_EVENT_ADJUSTED,
};

typedef void (*besim_clock_listener_t)(enum besim_clock_event event);

/* While the time is unset, set is false and the other fields are zero. */
void besim_clock_now(struct besim_clock_time *out);

/* Milliseconds until the minute changes. 60000 while the time is unset. */
uint32_t besim_clock_ms_to_next_minute(void);

/*
 * Both adjustments start from 00:00 while the time is unset, and both raise
 * BESIM_CLOCK_EVENT_ADJUSTED. Hours wrap within the day. Minutes wrap within
 * the hour without touching it, and restart the seconds at zero.
 */
void besim_clock_adjust_hours(int delta);
void besim_clock_adjust_minutes(int delta);

/* Raises BESIM_CLOCK_EVENT_SHOW. */
void besim_clock_request_show(void);

/*
 * One listener, called on the thread of whoever changed the clock. It must
 * not touch LVGL: it hands the work to the display work queue. NULL removes
 * the listener.
 */
void besim_clock_set_listener(besim_clock_listener_t cb);
