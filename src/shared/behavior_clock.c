/*
 * SPDX-License-Identifier: MIT
 *
 * ZMK behaviour that shows the shared clock and sets its time by hand. Based on
 * ZMK's outputs behaviour.
 */

#define DT_DRV_COMPAT zmk_behavior_oled_clock

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <drivers/behavior.h>

#include <dt-bindings/shared/clock.h>

#include <zmk/behavior.h>

#include <shared/clock.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static const struct behavior_parameter_value_metadata std_values[] = {
    {
        .value = CLK_SHOW,
        .display_name = "Show Clock",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
    },
    {
        .value = CLK_HOUR_INC,
        .display_name = "Hour +",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
    },
    {
        .value = CLK_HOUR_DEC,
        .display_name = "Hour -",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
    },
    {
        .value = CLK_MIN_INC,
        .display_name = "Minute +",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
    },
    {
        .value = CLK_MIN_DEC,
        .display_name = "Minute -",
        .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE,
    },
};

static const struct behavior_parameter_metadata_set std_set = {
    .param1_values = std_values,
    .param1_values_len = ARRAY_SIZE(std_values),
};

static const struct behavior_parameter_metadata metadata = {
    .sets_len = 1,
    .sets = &std_set,
};

#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    switch (binding->param1) {
    case CLK_SHOW:
        shared_clock_request_show();
        return ZMK_BEHAVIOR_OPAQUE;
    case CLK_HOUR_INC:
        shared_clock_adjust_hours(1);
        return ZMK_BEHAVIOR_OPAQUE;
    case CLK_HOUR_DEC:
        shared_clock_adjust_hours(-1);
        return ZMK_BEHAVIOR_OPAQUE;
    case CLK_MIN_INC:
        shared_clock_adjust_minutes(1);
        return ZMK_BEHAVIOR_OPAQUE;
    case CLK_MIN_DEC:
        shared_clock_adjust_minutes(-1);
        return ZMK_BEHAVIOR_OPAQUE;
    default:
        LOG_ERR("Unknown clock command: %d", binding->param1);
    }

    return -ENOTSUP;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_clock_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &metadata,
#endif // IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
};

BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_clock_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
