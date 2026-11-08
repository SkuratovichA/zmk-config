/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's key position event.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <zmk/event_manager.h>

#define ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL UINT8_MAX

struct zmk_position_state_changed {
    uint8_t source;
    uint32_t position;
    bool state;
    int64_t timestamp;
};

ZMK_EVENT_DECLARE(zmk_position_state_changed);
