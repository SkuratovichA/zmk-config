/*
 * SPDX-License-Identifier: MIT
 *
 * Which half this firmware is built for. A keyboard that is not split counts
 * as central.
 */

#pragma once

#include <zephyr/sys/util.h>

#define SHARED_IS_CENTRAL                                                                          \
    (!IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL))
