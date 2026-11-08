/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of Zephyr's sys/util.h: the helper macros the widget sources use, with IS_ENABLED
 * built on the same Z_IS_ENABLED trick as Zephyr's sys/util_macro.h.
 */

#pragma once

#include <stddef.h>
#include <zephyr/toolchain.h>

/* IS_ENABLED(CONFIG_X) is 1 when CONFIG_X is defined as 1 and 0 when it is not defined. */
#define IS_ENABLED(config_macro) Z_IS_ENABLED1(config_macro)
#define Z_IS_ENABLED1(config_macro) Z_IS_ENABLED2(_XXXX##config_macro)
#define _XXXX1 _YYYY,
#define Z_IS_ENABLED2(one_or_two_args) Z_IS_ENABLED3(one_or_two_args 1, 0)
#define Z_IS_ENABLED3(ignore_this, val, ...) val

#ifndef BUILD_ASSERT
#define BUILD_ASSERT(EXPR, ...) _Static_assert(EXPR, "" __VA_ARGS__)
#endif

#ifndef ARG_UNUSED
#define ARG_UNUSED(x) (void)(x)
#endif

/* Zephyr's BIT(): a constant expression, so it works in a static initializer. */
#ifndef BIT
#define BIT(n) (1UL << (n))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#endif

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(val, low, high) (((val) <= (low)) ? (low) : MIN(val, high))
#endif

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, field) ((type *)(((char *)(ptr)) - offsetof(type, field)))
#endif
