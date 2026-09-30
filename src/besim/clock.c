/*
 * SPDX-License-Identifier: MIT
 *
 * The wall clock of the besim status screens: a time set by hand and counted
 * from the uptime, with a listener that hears about shows and adjustments.
 */

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>

#include <besim/clock.h>

#define SECONDS_PER_DAY 86400
#define MS_PER_MINUTE 60000

static int64_t base_uptime_ms;
static uint32_t base_seconds_of_day;
static bool time_set;
static besim_clock_listener_t listener;
static struct k_spinlock lock;

/* The caller holds the lock and has checked that the time is set. */
static uint32_t seconds_of_day_now(void) {
    int64_t elapsed_s = (k_uptime_get() - base_uptime_ms) / 1000;

    return (uint32_t)(((int64_t)base_seconds_of_day + elapsed_s) % SECONDS_PER_DAY);
}

static int positive_mod(int value, int modulus) {
    return ((value % modulus) + modulus) % modulus;
}

static void notify(enum besim_clock_event event) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    besim_clock_listener_t cb = listener;
    k_spin_unlock(&lock, key);

    if (cb != NULL) {
        cb(event);
    }
}

void besim_clock_now(struct besim_clock_time *out) {
    k_spinlock_key_t key = k_spin_lock(&lock);

    if (time_set) {
        uint32_t seconds = seconds_of_day_now();

        out->set = true;
        out->hour = seconds / 3600;
        out->minute = (seconds % 3600) / 60;
        out->second = seconds % 60;
    } else {
        out->set = false;
        out->hour = 0;
        out->minute = 0;
        out->second = 0;
    }

    k_spin_unlock(&lock, key);
}

uint32_t besim_clock_ms_to_next_minute(void) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    uint32_t ms = MS_PER_MINUTE;

    if (time_set) {
        int64_t elapsed_ms = k_uptime_get() - base_uptime_ms;
        int64_t into_minute = ((int64_t)base_seconds_of_day * 1000 + elapsed_ms) % MS_PER_MINUTE;

        ms = (uint32_t)(MS_PER_MINUTE - into_minute);
    }

    k_spin_unlock(&lock, key);
    return ms;
}

/* Rewrites the base so that the clock reads the given time right now. */
static void store_time(int hour, int minute, int second) {
    base_uptime_ms = k_uptime_get();
    base_seconds_of_day = (uint32_t)(hour * 3600 + minute * 60 + second);
    time_set = true;
}

void besim_clock_adjust_hours(int delta) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    uint32_t seconds = time_set ? seconds_of_day_now() : 0;
    int hour = positive_mod((int)(seconds / 3600) + delta, 24);

    store_time(hour, (int)((seconds % 3600) / 60), (int)(seconds % 60));
    k_spin_unlock(&lock, key);

    notify(BESIM_CLOCK_EVENT_ADJUSTED);
}

void besim_clock_adjust_minutes(int delta) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    uint32_t seconds = time_set ? seconds_of_day_now() : 0;
    int minute = positive_mod((int)((seconds % 3600) / 60) + delta, 60);

    store_time((int)(seconds / 3600), minute, 0);
    k_spin_unlock(&lock, key);

    notify(BESIM_CLOCK_EVENT_ADJUSTED);
}

void besim_clock_request_show(void) {
    notify(BESIM_CLOCK_EVENT_SHOW);
}

void besim_clock_set_listener(besim_clock_listener_t cb) {
    k_spinlock_key_t key = k_spin_lock(&lock);
    listener = cb;
    k_spin_unlock(&lock, key);
}
