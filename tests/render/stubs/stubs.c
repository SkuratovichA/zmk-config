/*
 * SPDX-License-Identifier: MIT
 *
 * Function bodies of the Zephyr and ZMK stubs, implemented over one scenario state that main.c
 * edits directly, plus the event definitions and the harness's own event dispatch.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zmk/battery.h>
#include <zmk/behavior.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/split/central.h>
#include <zmk/usb.h>

#include <dt-bindings/zmk/bt.h>
/* Included last: it defines one-letter macros (A, B, C, ...). */
#include <dt-bindings/zmk/keys.h>

/* Clock and scenario ---------------------------------------------------------------------- */

int64_t harness_clock_ms = 1000;

int64_t k_uptime_get(void) { return harness_clock_ms; }

/* Delayable work -------------------------------------------------------------------------- */

#define HARNESS_MAX_DELAYABLE 16

/* Every delayable work that was ever armed; it stays listed and is run only while pending. */
static struct k_work_delayable *delayables[HARNESS_MAX_DELAYABLE];

static void track_delayable(struct k_work_delayable *dwork) {
    int free_slot = -1;

    for (int i = 0; i < HARNESS_MAX_DELAYABLE; i++) {
        if (delayables[i] == dwork) {
            return;
        }
        if (delayables[i] == NULL && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot < 0) {
        fprintf(stderr, "harness: too many delayable work items\n");
        abort();
    }
    delayables[free_slot] = dwork;
}

static void arm_delayable(struct k_work_delayable *dwork, k_timeout_t delay) {
    track_delayable(dwork);
    dwork->deadline_ms = harness_clock_ms + delay.ms;
    dwork->pending = true;
}

int k_work_schedule_for_queue(struct k_work_q *queue, struct k_work_delayable *dwork,
                              k_timeout_t delay) {
    (void)queue;
    if (dwork->pending) {
        return 0;
    }
    arm_delayable(dwork, delay);
    return 1;
}

int k_work_reschedule_for_queue(struct k_work_q *queue, struct k_work_delayable *dwork,
                                k_timeout_t delay) {
    (void)queue;
    arm_delayable(dwork, delay);
    return 1;
}

int k_work_cancel_delayable(struct k_work_delayable *dwork) {
    dwork->pending = false;
    return 0;
}

void harness_run_due_work(void) {
    for (int i = 0; i < HARNESS_MAX_DELAYABLE; i++) {
        struct k_work_delayable *dwork = delayables[i];

        if (dwork != NULL && dwork->pending && dwork->deadline_ms <= harness_clock_ms) {
            /* Not pending any more before the handler runs, so that it may arm itself again. */
            dwork->pending = false;
            dwork->work.handler(&dwork->work);
        }
    }
}

/* Display device --------------------------------------------------------------------------- */

const struct device harness_display_device = {.name = "harness-display"};

int display_set_contrast(const struct device *dev, uint8_t contrast) {
    (void)dev;
    harness_scenario.contrast = contrast;
    printf("contrast=%u\n", (unsigned)contrast);
    return 0;
}

/* The boot state is set by main.c; only the layer names are fixed here. */
struct harness_scenario harness_scenario = {
    .layer_names = {"BASE", "L1", "FN", "L3", "L4", "L5", "L6", "L7"},
};

/* Events ---------------------------------------------------------------------------------- */

/*
 * The real as_<event>() dereferences eh, which is harmless on the device when a widget's init
 * passes NULL, but a crash on the host, so NULL is answered with NULL here.
 */
#define HARNESS_EVENT_IMPL(t)                                                                      \
    const struct zmk_event_type zmk_event_##t = {.name = #t};                                      \
    struct t *as_##t(const zmk_event_t *eh) {                                                      \
        if (eh == NULL) {                                                                          \
            return NULL;                                                                           \
        }                                                                                          \
        return (eh->event == &zmk_event_##t) ? &((struct t##_event *)eh)->data : NULL;             \
    }

HARNESS_EVENT_IMPL(zmk_activity_state_changed)
HARNESS_EVENT_IMPL(zmk_battery_state_changed)
HARNESS_EVENT_IMPL(zmk_peripheral_battery_state_changed)
HARNESS_EVENT_IMPL(zmk_ble_active_profile_changed)
HARNESS_EVENT_IMPL(zmk_endpoint_changed)
HARNESS_EVENT_IMPL(zmk_layer_state_changed)
HARNESS_EVENT_IMPL(zmk_position_state_changed)
HARNESS_EVENT_IMPL(zmk_split_peripheral_status_changed)
HARNESS_EVENT_IMPL(zmk_usb_conn_state_changed)

#define HARNESS_MAX_SUBSCRIPTIONS 64

static struct zmk_event_subscription subscriptions[HARNESS_MAX_SUBSCRIPTIONS];
static int subscription_count;

void harness_subscribe(const struct zmk_event_type *type, const struct zmk_listener *listener) {
    if (subscription_count >= HARNESS_MAX_SUBSCRIPTIONS) {
        fprintf(stderr, "harness: too many event subscriptions\n");
        abort();
    }
    subscriptions[subscription_count].event_type = type;
    subscriptions[subscription_count].listener = listener;
    subscription_count++;
}

void harness_raise(const struct zmk_event_type *type, zmk_event_t *eh) {
    if (eh->event != type) {
        fprintf(stderr, "harness: event header does not match the raised type %s\n", type->name);
        abort();
    }
    for (int i = 0; i < subscription_count; i++) {
        if (subscriptions[i].event_type != type) {
            continue;
        }
        if (subscriptions[i].listener->callback(eh) != ZMK_EV_EVENT_BUBBLE) {
            break;
        }
    }
}

/* Display --------------------------------------------------------------------------------- */

static struct k_work_q display_work_q;

struct k_work_q *zmk_display_work_q(void) { return &display_work_q; }

bool zmk_display_is_initialized(void) { return harness_scenario.display_initialized; }

/* Battery, USB, endpoint, Bluetooth, split ------------------------------------------------- */

uint8_t zmk_battery_state_of_charge(void) { return harness_scenario.battery; }

enum zmk_usb_conn_state zmk_usb_get_conn_state(void) {
    return harness_scenario.usb_powered ? ZMK_USB_CONN_POWERED : ZMK_USB_CONN_NONE;
}

bool zmk_usb_is_hid_ready(void) { return zmk_usb_get_conn_state() == ZMK_USB_CONN_HID; }

struct zmk_endpoint_instance zmk_endpoint_get_selected(void) {
    struct zmk_endpoint_instance endpoint = {.transport = harness_scenario.transport};
    if (endpoint.transport == ZMK_TRANSPORT_BLE) {
        endpoint.ble.profile_index = harness_scenario.endpoint_ble_profile;
    }
    return endpoint;
}

int zmk_ble_active_profile_index(void) { return harness_scenario.ble_active_profile; }

bool zmk_ble_profile_is_connected(uint8_t index) {
    return index < HARNESS_PROFILES && harness_scenario.profile_connected[index];
}

bool zmk_ble_profile_is_open(uint8_t index) {
    return index >= HARNESS_PROFILES || harness_scenario.profile_open[index];
}

bool zmk_ble_active_profile_is_open(void) {
    return zmk_ble_profile_is_open(harness_scenario.ble_active_profile);
}

bool zmk_ble_active_profile_is_connected(void) {
    return zmk_ble_profile_is_connected(harness_scenario.ble_active_profile);
}

int zmk_split_central_get_peripheral_battery_level(uint8_t source, uint8_t *level) {
    if (source != 0 || !harness_scenario.peripheral_connected) {
        return -ENOTCONN;
    }
    *level = harness_scenario.peripheral_battery;
    return 0;
}

bool zmk_split_bt_peripheral_is_connected(void) { return harness_scenario.peripheral_connected; }

/* Keymap ---------------------------------------------------------------------------------- */

#define BINDING(dev, p1, p2) {.behavior_dev = dev, .param1 = p1, .param2 = p2}

static const struct zmk_behavior_binding base_layer[ZMK_KEYMAP_LEN] = {
    BINDING("key_press", A, 0),
    BINDING("key_press", ENTER, 0),
    BINDING("key_press", LS(B), 0),
    BINDING("key_press", C_VOL_UP, 0),
    BINDING("key_press", F5, 0),
    BINDING("momentary_layer", 2, 0),
    BINDING("bluetooth", BT_SEL_CMD, 1),
    BINDING("bootload", 0, 0),
};

/* Every position but 5 is transparent: a NULL behavior_dev stands for &trans. */
static const struct zmk_behavior_binding fn_layer[ZMK_KEYMAP_LEN] = {
    [5] = BINDING("key_press", ESC, 0),
};

zmk_keymap_layer_id_t zmk_keymap_layer_index_to_id(zmk_keymap_layer_index_t layer_index) {
    return layer_index;
}

bool zmk_keymap_layer_active(zmk_keymap_layer_id_t layer) {
    return layer < HARNESS_LAYERS && harness_scenario.layer_active[layer];
}

zmk_keymap_layer_index_t zmk_keymap_highest_layer_active(void) {
    for (int i = HARNESS_LAYERS - 1; i > 0; i--) {
        if (harness_scenario.layer_active[i]) {
            return i;
        }
    }
    return 0;
}

const char *zmk_keymap_layer_name(zmk_keymap_layer_id_t layer) {
    return layer < HARNESS_LAYERS ? harness_scenario.layer_names[layer] : NULL;
}

const struct zmk_behavior_binding *zmk_keymap_get_layer_binding_at_idx(zmk_keymap_layer_id_t layer,
                                                                       uint8_t binding_idx) {
    if (binding_idx >= ZMK_KEYMAP_LEN) {
        return NULL;
    }
    if (layer == 0) {
        return &base_layer[binding_idx];
    }
    if (layer == 2 && fn_layer[binding_idx].behavior_dev != NULL) {
        return &fn_layer[binding_idx];
    }
    return NULL;
}
