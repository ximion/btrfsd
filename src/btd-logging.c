/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:btd-logging
 * @short_description: Logging / syslog helpers
 *
 * Logging utility functions for Btrfsd.
 */

#include "config.h"
#include "btd-logging.h"

#include <glib.h>
#include <stdio.h>
#include <unistd.h>
#include <syslog.h>
#include <stdarg.h>
#ifdef HAVE_SYSTEMD
#include <systemd/sd-journal.h>
#include <systemd/sd-daemon.h>
#endif

typedef enum {
    LB_SYSLOG,
    LB_JOURNAL,
    LB_CONSOLE
} LogBackend;

static LogBackend _log_backend = LB_CONSOLE;

/**
 * btd_is_tty:
 *
 * returns: %TRUE if stdout is a tty.
 */
gboolean
btd_is_tty (void)
{
    return isatty (fileno (stdout));
}

/**
 * btd_logging_setup:
 * @verbose: %TRUE if verbose logging should be enabled.
 *
 * Setup logging facilities.
 */
void
btd_logging_setup (gboolean verbose)
{
    if (verbose)
        g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

    if (btd_is_tty ()) {
        /* only log to console if we have a tty */
        _log_backend = LB_CONSOLE;
        g_debug ("Logging to console.");
        return;
    }

#ifdef HAVE_SYSTEMD
    _log_backend = LB_JOURNAL;
    if (sd_booted () <= 0) {
        g_info ("Not booted with systemd, using syslog instead of journal.");
        _log_backend = LB_SYSLOG;
    } else {
        g_debug ("Logging to journal.");
    }
#endif

    if (_log_backend == LB_SYSLOG) {
        openlog ("btrfsd", LOG_PID | LOG_CONS, LOG_DAEMON);
        g_debug ("Logging to syslog.");
    }
}

/**
 * btd_logging_finalize:
 *
 * Finish logging.
 */
void
btd_logging_finalize (void)
{
    if (_log_backend == LB_SYSLOG)
        closelog ();
}

static void
btd_log_va (BtdLogLevel level, const gchar *format, va_list args)
{
    int log_priority;
    g_autofree gchar *formatted_message = NULL;

    formatted_message = g_strdup_vprintf (format, args);

    if (_log_backend == LB_CONSOLE) {
        switch (level) {
        case BTD_LOG_LEVEL_INFO:
            g_print ("I: %s\n", formatted_message);
            break;
        case BTD_LOG_LEVEL_WARNING:
            g_print ("W: %s\n", formatted_message);
            break;
        case BTD_LOG_LEVEL_ERROR:
            g_critical ("%s", formatted_message);
            break;
        default:
            g_debug ("%s", formatted_message);
            break;
        }
        return;
    }

    switch (level) {
    case BTD_LOG_LEVEL_INFO:
        log_priority = LOG_INFO;
        break;
    case BTD_LOG_LEVEL_WARNING:
        log_priority = LOG_WARNING;
        break;
    case BTD_LOG_LEVEL_ERROR:
        log_priority = LOG_ERR;
        break;
    default:
        log_priority = LOG_DEBUG;
        break;
    }

#ifdef HAVE_SYSTEMD
    if (_log_backend == LB_JOURNAL) {
        sd_journal_print (log_priority, "%s", formatted_message);
        return;
    }
#endif

    /* log with syslog if we haven't written the log message elsewhere */
    syslog (log_priority, "%s", formatted_message);
}

/**
 * btd_log:
 * @level: The log level.
 * @format: The string to log.
 *
 * Log a message.
 */
void
btd_log (BtdLogLevel level, const gchar *format, ...)
{
    va_list args;
    va_start (args, format);
    btd_log_va (level, format, args);
    va_end (args);
}

/**
 * btd_debug:
 *
 * Log a debug message.
 */
void
btd_debug (const gchar *format, ...)
{
    va_list args;
    va_start (args, format);
    btd_log_va (BTD_LOG_LEVEL_DEBUG, format, args);
    va_end (args);
}

/**
 * btd_info:
 *
 * Log an info message.
 */
void
btd_info (const gchar *format, ...)
{
    va_list args;
    va_start (args, format);
    btd_log_va (BTD_LOG_LEVEL_INFO, format, args);
    va_end (args);
}

/**
 * btd_warning:
 *
 * Log a warning message.
 */
void
btd_warning (const gchar *format, ...)
{
    va_list args;
    va_start (args, format);
    btd_log_va (BTD_LOG_LEVEL_WARNING, format, args);
    va_end (args);
}

/**
 * btd_error:
 *
 * Log an error message.
 */
void
btd_error (const gchar *format, ...)
{
    va_list args;
    va_start (args, format);
    btd_log_va (BTD_LOG_LEVEL_ERROR, format, args);
    va_end (args);
}
