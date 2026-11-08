/*
 * SPDX-License-Identifier: MIT
 *
 * One frame for the whole LED chain of a half. ZMK's underglow and the per-key
 * effect each own a part of the chain and write their pixels into this frame;
 * the frame goes to the strip from the system work queue, coalesced so that
 * the chain is written at most every CONFIG_SHARED_LED_FRAME_MIN_MS.
 *
 * Threads: the setters may be called from any thread (a spinlock guards the
 * frame); the strip is written on the system work queue only.
 */

#pragma once

#include <stddef.h>

#include <zephyr/drivers/led_strip.h>

/* Number of LEDs in the chain. */
size_t led_frame_length(void);

/* Sets one pixel, or a run of them, without sending anything. */
void led_frame_set(size_t index, struct led_rgb pixel);
void led_frame_set_range(size_t start, const struct led_rgb *pixels, size_t count);

/* Asks for the frame to be sent to the strip soon. */
void led_frame_flush(void);
