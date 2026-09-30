/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's Bluetooth header: the profile queries the status widget uses.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ZMK_BLE_PROFILE_COUNT CONFIG_BT_MAX_PAIRED

int zmk_ble_active_profile_index(void);
bool zmk_ble_profile_is_connected(uint8_t index);
bool zmk_ble_profile_is_open(uint8_t index);
bool zmk_ble_active_profile_is_open(void);
bool zmk_ble_active_profile_is_connected(void);
