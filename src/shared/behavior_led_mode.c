/*
 * SPDX-License-Identifier: MIT
 *
 * The LED mode key: each press moves on through off, underglow only, per-key
 * only, underglow and per-key. The underglow is ZMK's own; the per-key effect
 * is shared/key_rgb.c. Global locality, so one press sets both halves.
 */

#define DT_DRV_COMPAT zmk_behavior_led_mode

#include <stdbool.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/rgb_underglow.h>

#include <shared/key_rgb.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

enum led_mode {
    LED_MODE_OFF = 0,
    LED_MODE_UNDERGLOW = 1,
    LED_MODE_PER_KEY = 2,
    LED_MODE_BOTH = 3,
};

static enum led_mode current_mode(void) {
    bool underglow = false;
    zmk_rgb_underglow_get_state(&underglow);
    return (underglow ? LED_MODE_UNDERGLOW : 0) | (key_rgb_is_enabled() ? LED_MODE_PER_KEY : 0);
}

static void apply_mode(enum led_mode mode) {
    const bool underglow = (mode & LED_MODE_UNDERGLOW) != 0;
    const bool per_key = (mode & LED_MODE_PER_KEY) != 0;
    /* The underglow goes on before the per-key effect, off after it, so the
     * LED power rail is never cut under a running fade. */
    if (underglow) {
        zmk_rgb_underglow_on();
    }
    key_rgb_set_enabled(per_key);
    if (!underglow) {
        zmk_rgb_underglow_off();
    }
    LOG_INF("led mode %d", mode);
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    apply_mode((current_mode() + 1) % 4);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_led_mode_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_led_mode_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
