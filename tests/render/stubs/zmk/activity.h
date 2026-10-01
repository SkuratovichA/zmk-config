/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's activity header: the state enum only. The harness does not model ZMK's
 * activity tracking; the scenario raises the state events itself.
 */

#pragma once

enum zmk_activity_state {
    ZMK_ACTIVITY_ACTIVE,
    ZMK_ACTIVITY_IDLE,
    ZMK_ACTIVITY_SLEEP,
};
