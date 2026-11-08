/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of ZMK's behavior header: the binding struct as it is without local ids. The
 * dt-bindings/zmk/keys.h key names are deliberately not included here, because they define
 * one-letter macros (A, B, C, ...) that would corrupt the LVGL headers included after this one;
 * stubs.c includes them on its own, last.
 */

#pragma once

#include <stdint.h>

struct zmk_behavior_binding {
    const char *behavior_dev;
    uint32_t param1;
    uint32_t param2;
};
