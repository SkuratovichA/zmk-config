/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's endpoint types, as they are in ZMK.
 */

#pragma once

enum zmk_transport {
    ZMK_TRANSPORT_NONE = 0,
    ZMK_TRANSPORT_USB = 1,
    ZMK_TRANSPORT_BLE = 2,
};

struct zmk_transport_usb_data {};

struct zmk_transport_ble_data {
    int profile_index;
};

struct zmk_endpoint_instance {
    enum zmk_transport transport;
    union {
        struct zmk_transport_usb_data usb;
        struct zmk_transport_ble_data ble;
    };
};
