/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's activity state event.
 */

#pragma once

#include <zmk/activity.h>
#include <zmk/event_manager.h>

struct zmk_activity_state_changed {
    enum zmk_activity_state state;
};

ZMK_EVENT_DECLARE(zmk_activity_state_changed);
