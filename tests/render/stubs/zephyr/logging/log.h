/*
 * SPDX-License-Identifier: MIT
 *
 * Host stub of Zephyr's logging: module declarations and log calls do nothing.
 */

#pragma once

/* Ends in a declaration so that the semicolon after the macro use stays valid. */
#define LOG_MODULE_DECLARE(...) extern int harness_log_module_unused

#define LOG_DBG(...) do { } while (0)
#define LOG_INF(...) do { } while (0)
#define LOG_WRN(...) do { } while (0)
#define LOG_ERR(...) do { } while (0)
