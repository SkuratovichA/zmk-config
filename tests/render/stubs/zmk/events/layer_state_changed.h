/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's layer state event.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zmk/event_manager.h>

struct zmk_layer_state_changed {
    uint8_t layer;
    bool state;
    bool locked;
    int64_t timestamp;
};

ZMK_EVENT_DECLARE(zmk_layer_state_changed);
