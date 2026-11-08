/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's event manager: the event types and listener registration without the
 * raise machinery, plus the harness additions at the bottom (a raise function that calls the
 * subscribed listeners, and the scenario state that the stubs report and main.c edits).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/toolchain.h>
#include <zephyr/types.h>

struct zmk_event_type {
    const char *name;
};

typedef struct {
    const struct zmk_event_type *event;
    uint8_t last_listener_index;
} zmk_event_t;

#define ZMK_EV_EVENT_BUBBLE 0
#define ZMK_EV_EVENT_HANDLED 1
#define ZMK_EV_EVENT_CAPTURED 2

typedef int (*zmk_listener_callback_t)(const zmk_event_t *eh);

struct zmk_listener {
    zmk_listener_callback_t callback;
};

struct zmk_event_subscription {
    const struct zmk_event_type *event_type;
    const struct zmk_listener *listener;
};

/* The definitions live in stubs.c through HARNESS_EVENT_IMPL. */
#define ZMK_EVENT_DECLARE(event_type)                                                              \
    struct event_type##_event {                                                                    \
        zmk_event_t header;                                                                        \
        struct event_type data;                                                                    \
    };                                                                                             \
    struct event_type *as_##event_type(const zmk_event_t *eh);                                     \
    extern const struct zmk_event_type zmk_event_##event_type;

#define ZMK_LISTENER(mod, cb) const struct zmk_listener zmk_listener_##mod = {.callback = cb};

/* Registers the (event type, listener) pair at load time, so harness_raise can find it. */
#define ZMK_SUBSCRIPTION(mod, ev_type)                                                             \
    extern const struct zmk_listener zmk_listener_##mod;                                           \
    __attribute__((constructor)) static void _CONCAT(_CONCAT(harness_subscribe_, mod),             \
                                                     _CONCAT(_, ev_type))(void) {                  \
        harness_subscribe(&zmk_event_##ev_type, &zmk_listener_##mod);                              \
    }

/* Harness additions ---------------------------------------------------------------------- */

void harness_subscribe(const struct zmk_event_type *type, const struct zmk_listener *listener);

/* Calls every listener subscribed to type, in registration order, until one does not bubble. */
void harness_raise(const struct zmk_event_type *type, zmk_event_t *eh);

/* Builds the event around data (a braced initializer of struct t) and raises it. */
#define HARNESS_RAISE(t, ...)                                                                      \
    do {                                                                                           \
        struct t##_event harness_ev_ = {.header = {.event = &zmk_event_##t}, .data = __VA_ARGS__}; \
        harness_raise(&zmk_event_##t, &harness_ev_.header);                                        \
    } while (0)

#define HARNESS_PROFILES 5
#define HARNESS_LAYERS 8

/* What the stubbed keyboard reports; main.c edits it directly and then raises the event. */
struct harness_scenario {
    uint8_t battery;
    bool usb_powered;
    uint8_t peripheral_battery;
    bool peripheral_connected;
    int transport;                  /* enum zmk_transport */
    int endpoint_ble_profile;       /* profile index of the BLE endpoint */
    int ble_active_profile;
    bool profile_connected[HARNESS_PROFILES];
    bool profile_open[HARNESS_PROFILES];
    bool layer_active[HARNESS_LAYERS];
    const char *layer_names[HARNESS_LAYERS];
    bool display_initialized;
    int contrast;                   /* the last value passed to display_set_contrast */
};

extern struct harness_scenario harness_scenario;
