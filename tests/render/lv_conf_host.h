/*
 * SPDX-License-Identifier: MIT
 *
 * The host counterpart of Zephyr's LVGL glue lv_conf.h: everything else comes from the
 * CONFIG_LV_* macros of autoconf_lv.h, which lv_conf_internal.h reads, so the string and
 * sprintf functions stay LVGL's builtin ones as configured there.
 */

#ifndef LV_CONF_HOST_H
#define LV_CONF_HOST_H

/* Memory: the C library instead of Zephyr's heap. */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB

/* Provide the alignment of LVGL buffers. */
#define LV_ATTRIBUTE_MEM_ALIGN __attribute__((aligned(4)))

/* A failed LVGL assert aborts the harness. */
#define LV_ASSERT_HANDLER abort();
#define LV_ASSERT_HANDLER_INCLUDE "stdlib.h"

#define LV_CONF_SUPPRESS_DEFINE_CHECK 1

#define LV_USE_OS LV_OS_NONE

#endif /* LV_CONF_HOST_H */
