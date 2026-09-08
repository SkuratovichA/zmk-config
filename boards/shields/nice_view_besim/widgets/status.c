/*
 * SPDX-License-Identifier: MIT
 *
 * Central (left half) status screen, top to bottom:
 *   - battery of this half and of the right half, in percent
 *   - output: USB, or Bluetooth profile number with its state
 *   - the five Bluetooth profile slots
 *   - the mascot
 *   - the active layer's name
 */

#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
#include <zmk/split/central.h>
#endif

#include "mascot_widget.h"
#include "status.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool profiles_connected[NICEVIEW_PROFILE_COUNT];
    bool profiles_bonded[NICEVIEW_PROFILE_COUNT];
};

struct layer_status_state {
    zmk_keymap_layer_index_t index;
    const char *label;
};

struct peripheral_battery_status_state {
    bool known;
    uint8_t level;
};

static void draw_top(lv_obj_t *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_16, LV_TEXT_ALIGN_CENTER);
    lv_draw_arc_dsc_t arc_ring_dsc;
    init_arc_dsc(&arc_ring_dsc, LVGL_FOREGROUND, 1);
    lv_draw_arc_dsc_t arc_filled_dsc;
    init_arc_dsc(&arc_filled_dsc, LVGL_FOREGROUND, 6);
    lv_draw_arc_dsc_t arc_dot_dsc;
    init_arc_dsc(&arc_dot_dsc, LVGL_FOREGROUND, 2);

    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);

    // Both batteries
    draw_battery_row(canvas, 0, "L", state->battery, state->charging, true);
    draw_battery_row(canvas, 13, "R", state->peripheral_battery, false, state->peripheral_known);

    // Output: "USB" or "BT <profile> <state>"
    char output_text[24] = {};
    switch (state->selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        strcpy(output_text, LV_SYMBOL_USB " USB");
        break;
    case ZMK_TRANSPORT_BLE: {
        const char *profile_state = LV_SYMBOL_SETTINGS; // open: pairing / advertising
        if (state->active_profile_bonded) {
            profile_state = state->active_profile_connected ? LV_SYMBOL_OK : LV_SYMBOL_CLOSE;
        }
        snprintf(output_text, sizeof(output_text), LV_SYMBOL_BLUETOOTH " %d %s",
                 state->active_profile_index + 1, profile_state);
        break;
    }
    default:
        strcpy(output_text, LV_SYMBOL_CLOSE);
        break;
    }
    canvas_draw_text(canvas, 0, 27, CANVAS_SIZE, &label_dsc, output_text);

    // Profile slots 1..5: solid = active, ring with a dot = paired, empty ring = free
    const bool on_ble = state->selected_endpoint.transport == ZMK_TRANSPORT_BLE;
    for (int i = 0; i < NICEVIEW_PROFILE_COUNT; i++) {
        const lv_coord_t cx = 8 + i * 13;
        const lv_coord_t cy = 58;
        if (on_ble && i == state->active_profile_index) {
            canvas_draw_arc(canvas, cx, cy, 6, 0, 359, &arc_filled_dsc);
        } else {
            canvas_draw_arc(canvas, cx, cy, 6, 0, 360, &arc_ring_dsc);
            if (state->profiles_bonded[i]) {
                canvas_draw_arc(canvas, cx, cy, 2, 0, 359, &arc_dot_dsc);
            }
        }
    }

    rotate_canvas(canvas);
}

static void draw_bottom(lv_obj_t *widget, const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 1);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_montserrat_14, LV_TEXT_ALIGN_CENTER);

    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);

    if (state->layer_label == NULL || strlen(state->layer_label) == 0) {
        char text[12] = {};
        snprintf(text, sizeof(text), "LAYER %u", state->layer_index);
        canvas_draw_text(canvas, 0, 5, CANVAS_SIZE, &label_dsc, text);
    } else {
        canvas_draw_text(canvas, 0, 5, CANVAS_SIZE, &label_dsc, state->layer_label);
    }

    rotate_canvas(canvas);
}

/* Battery of this half ------------------------------------------------- */

static void set_battery_status(struct zmk_widget_status *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_top(widget->obj, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev =
        (eh != NULL) ? as_zmk_battery_state_changed(eh) : NULL;
    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif

/* Battery of the right half ------------------------------------------- */

#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)

static void set_peripheral_battery_status(struct zmk_widget_status *widget,
                                          struct peripheral_battery_status_state state) {
    widget->state.peripheral_known = state.known;
    widget->state.peripheral_battery = state.level;
    draw_top(widget->obj, &widget->state);
}

static void peripheral_battery_status_update_cb(struct peripheral_battery_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_peripheral_battery_status(widget, state);
    }
}

static struct peripheral_battery_status_state
peripheral_battery_status_get_state(const zmk_event_t *eh) {
    if (eh != NULL) {
        const struct zmk_peripheral_battery_state_changed *ev =
            as_zmk_peripheral_battery_state_changed(eh);
        if (ev != NULL) {
            return (struct peripheral_battery_status_state){.known = true,
                                                            .level = ev->state_of_charge};
        }
    }
    uint8_t level = 0;
    if (zmk_split_central_get_peripheral_battery_level(0, &level) == 0 && level > 0) {
        return (struct peripheral_battery_status_state){.known = true, .level = level};
    }
    return (struct peripheral_battery_status_state){.known = false, .level = 0};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_battery_status,
                            struct peripheral_battery_status_state,
                            peripheral_battery_status_update_cb,
                            peripheral_battery_status_get_state)
ZMK_SUBSCRIPTION(widget_peripheral_battery_status, zmk_peripheral_battery_state_changed);

#endif /* CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING */

/* Output / Bluetooth profile ------------------------------------------- */

static void set_output_status(struct zmk_widget_status *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;
    for (int i = 0; i < NICEVIEW_PROFILE_COUNT; ++i) {
        widget->state.profiles_connected[i] = state->profiles_connected[i];
        widget->state.profiles_bonded[i] = state->profiles_bonded[i];
    }
    draw_top(widget->obj, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, &state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    struct output_status_state state = {
        .selected_endpoint = zmk_endpoint_get_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
    for (int i = 0; i < MIN(NICEVIEW_PROFILE_COUNT, ZMK_BLE_PROFILE_COUNT); ++i) {
        state.profiles_connected[i] = zmk_ble_profile_is_connected(i);
        state.profiles_bonded[i] = !zmk_ble_profile_is_open(i);
    }
    return state;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/* Layer ---------------------------------------------------------------- */

static void set_layer_status(struct zmk_widget_status *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;
    draw_bottom(widget->obj, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){
        .index = index, .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index))};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

/* ---------------------------------------------------------------------- */

int zmk_widget_status_init(struct zmk_widget_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 160, 68);

    // child 0: top band
    lv_obj_t *top = lv_canvas_create(widget->obj);
    lv_obj_align(top, LV_ALIGN_TOP_LEFT, BAND_TOP_X, 0);
    lv_canvas_set_buffer(top, widget->cbuf_top, CANVAS_SIZE, CANVAS_SIZE, CANVAS_COLOR_FORMAT);

    // child 1: bottom band (layer name)
    lv_obj_t *bottom = lv_canvas_create(widget->obj);
    lv_obj_align(bottom, LV_ALIGN_TOP_LEFT, BAND_BOTTOM_X, 0);
    lv_canvas_set_buffer(bottom, widget->cbuf_bottom, CANVAS_SIZE, CANVAS_SIZE,
                         CANVAS_COLOR_FORMAT);

    // child 2: mascot
    zmk_widget_mascot_init(widget->obj, BAND_MIDDLE_X);

    sys_slist_append(&widgets, &widget->node);

    widget_battery_status_init();
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    widget_peripheral_battery_status_init();
#endif
    widget_output_status_init();
    widget_layer_status_init();

    return 0;
}

lv_obj_t *zmk_widget_status_obj(struct zmk_widget_status *widget) { return widget->obj; }
