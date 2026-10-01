/*
 * SPDX-License-Identifier: MIT
 *
 * State getters shared by the shared status screens. Each one takes the event
 * that triggered it, or NULL to read the current state, which is the form a
 * state function of ZMK_DISPLAY_WIDGET_LISTENER has.
 *
 * The getters exist on the central half only. A peripheral sees the battery
 * state struct and nothing else.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/sys/util.h>

#include <shared/role.h>
#include <zmk/event_manager.h>

#define SHARED_PROFILE_COUNT 5

struct shared_battery_status_state {
    uint8_t level;
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    bool usb_present;
#endif
};

#if SHARED_IS_CENTRAL

#include <zmk/endpoints.h>
#include <zmk/keymap.h>

struct shared_peripheral_battery_status_state {
    bool known;
    uint8_t level;
};

struct shared_output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool profiles_connected[SHARED_PROFILE_COUNT];
    bool profiles_bonded[SHARED_PROFILE_COUNT];
};

struct shared_layer_status_state {
    zmk_keymap_layer_index_t index;
    const char *label;
};

/* The level comes from the event when there is one, else from the battery. */
struct shared_battery_status_state shared_battery_status_get_state(const zmk_event_t *eh);

/* Without an event a level of zero counts as unknown. */
struct shared_peripheral_battery_status_state
shared_peripheral_battery_status_get_state(const zmk_event_t *eh);

struct shared_output_status_state shared_output_status_get_state(const zmk_event_t *eh);

/* The highest active layer and its display name, which may be NULL. */
struct shared_layer_status_state shared_layer_status_get_state(const zmk_event_t *eh);

#endif /* SHARED_IS_CENTRAL */
