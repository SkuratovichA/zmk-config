/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of Zephyr's device model: one device object, the stub display, whatever node the
 * sources ask for.
 */

#pragma once

struct device {
    const char *name;
};

/* Defined in stubs.c. */
extern const struct device harness_display_device;

/* The node argument is never used: every devicetree lookup answers with the stub display. */
#define DEVICE_DT_GET(node) (&harness_display_device)
