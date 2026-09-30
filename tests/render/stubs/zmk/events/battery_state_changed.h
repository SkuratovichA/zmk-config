/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's battery events: this half's level and the peripheral's level.
 */

#pragma once

#include <stdint.h>
#include <zmk/event_manager.h>

struct zmk_battery_state_changed {
    uint8_t state_of_charge;
};

ZMK_EVENT_DECLARE(zmk_battery_state_changed);

struct zmk_peripheral_battery_state_changed {
    uint8_t source;
    uint8_t state_of_charge;
};

ZMK_EVENT_DECLARE(zmk_peripheral_battery_state_changed);
