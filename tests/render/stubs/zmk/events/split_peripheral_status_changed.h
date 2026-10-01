/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's split peripheral connection event.
 */

#pragma once

#include <stdbool.h>
#include <zmk/event_manager.h>

struct zmk_split_peripheral_status_changed {
    bool connected;
};

ZMK_EVENT_DECLARE(zmk_split_peripheral_status_changed);
