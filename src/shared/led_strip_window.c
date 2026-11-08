/*
 * SPDX-License-Identifier: MIT
 *
 * A LED strip device that is a window onto part of the LED frame. ZMK's
 * underglow is pointed at it (chosen zmk,underglow), so its effects reach only
 * the LEDs of the window and the per-key effect keeps the rest of the chain.
 */

#define DT_DRV_COMPAT skuratovich_led_strip_window

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/sys/util.h>

#include <shared/led_frame.h>

struct window_config {
    size_t offset;
    size_t length;
};

static int window_update_rgb(const struct device *dev, struct led_rgb *pixels,
                             size_t num_pixels) {
    const struct window_config *cfg = dev->config;
    led_frame_set_range(cfg->offset, pixels, MIN(num_pixels, cfg->length));
    led_frame_flush();
    return 0;
}

static int window_update_channels(const struct device *dev, uint8_t *channels,
                                  size_t num_channels) {
    ARG_UNUSED(dev);
    ARG_UNUSED(channels);
    ARG_UNUSED(num_channels);
    return -ENOTSUP;
}

static size_t window_length(const struct device *dev) {
    const struct window_config *cfg = dev->config;
    return cfg->length;
}

static const struct led_strip_driver_api window_api = {
    .update_rgb = window_update_rgb,
    .update_channels = window_update_channels,
    .length = window_length,
};

static int window_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

static const struct window_config window_config_0 = {
    .offset = DT_INST_PROP(0, offset),
    .length = DT_INST_PROP(0, chain_length),
};

DEVICE_DT_INST_DEFINE(0, window_init, NULL, NULL, &window_config_0, POST_KERNEL,
                      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &window_api);
