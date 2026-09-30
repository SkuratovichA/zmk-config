/*
 * SPDX-License-Identifier: MIT
 *
 * The part of the pressed-keys widget that does not draw. It listens for key
 * positions, turns each one into a short label and keeps the last few keys in
 * slots. A status screen registers a work item and draws from a snapshot.
 *
 * Threads: the position listener runs on the thread that raised the event.
 * keys_core_snapshot() is meant for the display work queue. A lock guards
 * the slots, so the two never see a half-written slot.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>

#define KEYS_SLOTS 6
/* Size of a label with its terminator, so a label has up to five characters. */
#define KEYS_LABEL_MAX 6

/* Period of the renderer's redraw timer. */
#define KEYS_TICK_MS 100
/* How long a fresh press keeps its outline. */
#define KEYS_POP_MS 120
/* How long a released key stays on the screen as a ghost. */
#define KEYS_GHOST_MS 600

struct keys_slot_view {
    bool used;
    bool held;
    int64_t pressed_at;
    int64_t released_at;
    char label[KEYS_LABEL_MAX];
};

/*
 * Copies the slots into out, after freeing every ghost that was released
 * KEYS_GHOST_MS or longer before now. now is a k_uptime_get() value. Returns
 * true when any slot is still used.
 */
bool keys_core_snapshot(struct keys_slot_view out[KEYS_SLOTS], int64_t now);

/*
 * The work item that the listener submits to the display work queue on each
 * key event, once the display is initialised. NULL stops the submissions.
 */
void keys_core_set_redraw_work(struct k_work *work);

/*
 * Called on the event thread for every key press, after the binding was
 * resolved. clock_key tells whether the key is bound to the besim clock
 * behaviour. It must not touch LVGL. NULL, the default, disables the call.
 */
typedef void (*keys_core_press_cb_t)(bool clock_key);
void keys_core_set_press_callback(keys_core_press_cb_t cb);
