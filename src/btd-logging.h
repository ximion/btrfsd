/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * BtdLogLevel:
 * @BTD_LOG_LEVEL_DEBUG:   Debug level
 * @BTD_LOG_LEVEL_INFO:    Info level
 * @BTD_LOG_LEVEL_WARNING: Warning level
 * @BTD_LOG_LEVEL_ERROR:   Error level
 *
 * The log message priority.
 **/
typedef enum {
    BTD_LOG_LEVEL_DEBUG,
    BTD_LOG_LEVEL_INFO,
    BTD_LOG_LEVEL_WARNING,
    BTD_LOG_LEVEL_ERROR,
    /*< private >*/
    BTD_LOG_LEVEL_LAST
} BtdLogLevel;

gboolean btd_is_tty (void);

void     btd_logging_setup (gboolean verbose);
void     btd_logging_finalize (void);

void     btd_log (BtdLogLevel level, const gchar *format, ...) G_GNUC_PRINTF (2, 3);

void     btd_debug (const gchar *format, ...) G_GNUC_PRINTF (1, 2);
void     btd_info (const gchar *format, ...) G_GNUC_PRINTF (1, 2);
void     btd_warning (const gchar *format, ...) G_GNUC_PRINTF (1, 2);
void     btd_error (const gchar *format, ...) G_GNUC_PRINTF (1, 2);

G_END_DECLS
