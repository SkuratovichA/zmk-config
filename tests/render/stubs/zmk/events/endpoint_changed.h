/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's selected endpoint event.
 */

#pragma once

#include <zmk/endpoints.h>
#include <zmk/event_manager.h>

struct zmk_endpoint_changed {
    struct zmk_endpoint_instance endpoint;
};

ZMK_EVENT_DECLARE(zmk_endpoint_changed);
