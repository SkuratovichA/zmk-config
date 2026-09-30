/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's split central header: the peripheral battery query.
 */

#pragma once

#include <stdint.h>

int zmk_split_central_get_peripheral_battery_level(uint8_t source, uint8_t *level);
