/*
 * SPDX-License-Identifier: MIT
 *
 * State getters shared by the shared status screens: battery of this half and of the other half,
 * output and Bluetooth profiles, active layer. They exist on the central half only.
 */

#include <shared/status_state.h>

#if SHARED_IS_CENTRAL

#include <zephyr/kernel.h>

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
#include <zmk/split/central.h>
#endif

/* Battery of this half ------------------------------------------------- */

struct shared_battery_status_state shared_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev =
        (eh != NULL) ? as_zmk_battery_state_changed(eh) : NULL;
    return (struct shared_battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

/* Battery of the other half -------------------------------------------- */

struct shared_peripheral_battery_status_state
shared_peripheral_battery_status_get_state(const zmk_event_t *eh) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING)
    if (eh != NULL) {
        const struct zmk_peripheral_battery_state_changed *ev =
            as_zmk_peripheral_battery_state_changed(eh);
        if (ev != NULL) {
            return (struct shared_peripheral_battery_status_state){.known = true,
                                                                  .level = ev->state_of_charge};
        }
    }
    uint8_t level = 0;
    if (zmk_split_central_get_peripheral_battery_level(0, &level) == 0 && level > 0) {
        return (struct shared_peripheral_battery_status_state){.known = true, .level = level};
    }
#endif
    return (struct shared_peripheral_battery_status_state){.known = false, .level = 0};
}

/* Output / Bluetooth profile ------------------------------------------- */

struct shared_output_status_state shared_output_status_get_state(const zmk_event_t *_eh) {
    struct shared_output_status_state state = {
        .selected_endpoint = zmk_endpoint_get_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
    for (int i = 0; i < MIN(SHARED_PROFILE_COUNT, ZMK_BLE_PROFILE_COUNT); ++i) {
        state.profiles_connected[i] = zmk_ble_profile_is_connected(i);
        state.profiles_bonded[i] = !zmk_ble_profile_is_open(i);
    }
    return state;
}

/* Layer ---------------------------------------------------------------- */

struct shared_layer_status_state shared_layer_status_get_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    return (struct shared_layer_status_state){
        .index = index, .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index))};
}

#endif /* SHARED_IS_CENTRAL */
