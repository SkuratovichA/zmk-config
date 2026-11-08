/*
 * SPDX-License-Identifier: MIT
 *
 * The LED frame: the one place that writes the physical LED chain. See
 * include/shared/led_frame.h.
 */

#define DT_DRV_COMPAT skuratovich_led_frame

#include <stdbool.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/util.h>

#include <shared/led_frame.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define STRIP_NODE DT_INST_PHANDLE(0, strip)
#define FRAME_LEN DT_PROP(STRIP_NODE, chain_length)

static const struct device *const strip = DEVICE_DT_GET(STRIP_NODE);
static struct led_rgb frame[FRAME_LEN];
static struct led_rgb outgoing[FRAME_LEN];
static struct k_spinlock lock;
static bool dirty;
static bool ready;

static void flush_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(flush_work, flush_handler);

size_t led_frame_length(void) { return FRAME_LEN; }

void led_frame_set(size_t index, struct led_rgb pixel) {
    if (index >= FRAME_LEN) {
        return;
    }
    k_spinlock_key_t key = k_spin_lock(&lock);
    frame[index] = pixel;
    dirty = true;
    k_spin_unlock(&lock, key);
}

void led_frame_set_range(size_t start, const struct led_rgb *pixels, size_t count) {
    if (start >= FRAME_LEN) {
        return;
    }
    count = MIN(count, FRAME_LEN - start);
    k_spinlock_key_t key = k_spin_lock(&lock);
    memcpy(&frame[start], pixels, count * sizeof(struct led_rgb));
    dirty = true;
    k_spin_unlock(&lock, key);
}

void led_frame_flush(void) {
    if (!ready) {
        return;
    }
    /* A pending flush picks the newest frame up anyway; a running one reschedules. */
    k_work_schedule(&flush_work, K_NO_WAIT);
}

static void flush_handler(struct k_work *work) {
    ARG_UNUSED(work);

    k_spinlock_key_t key = k_spin_lock(&lock);
    memcpy(outgoing, frame, sizeof(outgoing));
    dirty = false;
    k_spin_unlock(&lock, key);

    const int rc = led_strip_update_rgb(strip, outgoing, FRAME_LEN);
    if (rc != 0) {
        LOG_WRN("led strip update failed: %d", rc);
    }

    key = k_spin_lock(&lock);
    const bool again = dirty;
    k_spin_unlock(&lock, key);
    if (again) {
        k_work_schedule(&flush_work, K_MSEC(CONFIG_SHARED_LED_FRAME_MIN_MS));
    }
}

static int led_frame_init(void) {
    if (!device_is_ready(strip)) {
        LOG_ERR("led strip not ready, the LED frame stays off");
        return -ENODEV;
    }
    ready = true;
    /* Whatever the previous firmware left lit goes dark. */
    dirty = true;
    k_work_schedule(&flush_work, K_NO_WAIT);
    return 0;
}

SYS_INIT(led_frame_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
