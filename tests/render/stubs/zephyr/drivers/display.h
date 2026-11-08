/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of Zephyr's display driver API: the contrast call, which stubs.c records in the
 * scenario state and prints, so the run log shows every contrast change.
 */

#pragma once

#include <stdint.h>

#include <zephyr/device.h>

int display_set_contrast(const struct device *dev, uint8_t contrast);
