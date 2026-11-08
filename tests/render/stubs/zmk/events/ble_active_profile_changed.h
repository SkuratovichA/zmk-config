/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's active Bluetooth profile event.
 */

#pragma once

#include <stdint.h>
#include <zmk/event_manager.h>

struct zmk_ble_profile;

struct zmk_ble_active_profile_changed {
    uint8_t index;
    struct zmk_ble_profile *profile;
};

ZMK_EVENT_DECLARE(zmk_ble_active_profile_changed);
