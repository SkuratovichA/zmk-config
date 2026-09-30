/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of the Zephyr toolchain macros the widget sources and the stubs use.
 */

#pragma once

/* The macOS SDK's sys/cdefs.h defines __used and __unused too, with the same meaning. */
#ifndef __used
#define __used __attribute__((used))
#endif
#ifndef __unused
#define __unused __attribute__((unused))
#endif
#define __aligned(x) __attribute__((aligned(x)))

#ifndef __maybe_unused
#define __maybe_unused __attribute__((unused))
#endif

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

#define _CONCAT_(x, y) x##y
#define _CONCAT(x, y) _CONCAT_(x, y)
