/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of Zephyr's kernel API: work items run at once on submit, mutexes do nothing, and
 * the uptime is the harness's virtual clock, which the scenario in main.c advances.
 */

#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/sys/slist.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>
#include <zephyr/types.h>

struct k_work {
    void (*handler)(struct k_work *work);
};

struct k_work_q {
    int dummy;
};

/* Like Zephyr's, these define a global: a source that wants internal linkage writes "static". */
#define K_WORK_DEFINE(name, fn) struct k_work name = {.handler = fn}

/* A timeout is a number of milliseconds on the harness's virtual clock. */
typedef struct {
    int64_t ms;
} k_timeout_t;

#define K_MSEC(t) ((k_timeout_t){.ms = (int64_t)(t)})

/* Delayable work does not run on submit: it runs from harness_run_due_work() once due. */
struct k_work_delayable {
    struct k_work work;
    int64_t deadline_ms;
    bool pending;
};

#define K_WORK_DELAYABLE_DEFINE(name, fn) struct k_work_delayable name = {.work = {.handler = fn}}

/* Arms the work when it is not pending and returns 1; a pending work is left alone, returns 0. */
struct k_work_q;
int k_work_schedule_for_queue(struct k_work_q *queue, struct k_work_delayable *dwork,
                              k_timeout_t delay);

/* Always arms the work anew, whether or not it was pending; returns 1. */
int k_work_reschedule_for_queue(struct k_work_q *queue, struct k_work_delayable *dwork,
                                k_timeout_t delay);

int k_work_cancel_delayable(struct k_work_delayable *dwork);

/* Runs every armed work whose deadline has passed; main.c calls it on each virtual tick. */
void harness_run_due_work(void);

static inline int k_work_submit_to_queue(struct k_work_q *queue, struct k_work *work) {
    (void)queue;
    work->handler(work);
    return 0;
}

#define K_FOREVER 0
#define K_MUTEX_DEFINE(name) static int name

static inline int k_mutex_lock(int *mutex, int timeout) {
    (void)mutex;
    (void)timeout;
    return 0;
}

static inline int k_mutex_unlock(int *mutex) {
    (void)mutex;
    return 0;
}

/* The virtual clock in milliseconds; the same value feeds lv_tick_set_cb in main.c. */
extern int64_t harness_clock_ms;

int64_t k_uptime_get(void);
