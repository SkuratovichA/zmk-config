/*
 * SPDX-License-Identifier: MIT
 *
 * The reactive per-key RGB effect: a pressed key lights its LED and fades out.
 * The switch is kept in the settings, so it survives a reboot.
 */

#pragma once

#include <stdbool.h>

bool key_rgb_is_enabled(void);

/* Switching it off ends every running fade at once. Any thread. */
void key_rgb_set_enabled(bool enabled);
