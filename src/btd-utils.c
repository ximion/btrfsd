/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:btd-utils
 * @short_description: Various utility functions.
 *
 * Utility functions for Btrfsd.
 */

#include "config.h"
#include "btd-utils.h"

#include "btd-resources.h"

/**
 * btd_is_empty:
 * @str: The string to test.
 *
 * Test if a C string is %NULL or empty (contains only terminating null character).
 *
 * Returns: %TRUE if string was empty.
 */
gboolean
btd_is_empty (const gchar *str)
{
    if ((str == NULL) || (str[0] == '\0'))
        return TRUE;
    return FALSE;
}

/**
 * btd_strstripnl:
 * @string: a string to remove surrounding whitespaces and newlines
 *
 * Removes newlines and whitespaces surrounding a string.
 *
 * This function doesn't allocate or reallocate any memory;
 * it modifies @string in place.
 *
 * As opposed to g_strstrip() this function also removes newlines
 * from the start and end of strings.
 *
 * Returns: @string
 */
gchar *
btd_strstripnl (gchar *string)
{
    gsize len;
    guchar *start;
    if (string == NULL)
        return NULL;

    /* remove trailing whitespaces/newlines */
    len = strlen (string);
    while (len--) {
        const guchar c = string[len];
        if (g_ascii_isspace (c) || (c == '\n'))
            string[len] = '\0';
        else
            break;
    }

    /* remove leading whitespaces/newlines */
    for (start = (guchar *) string; *start && (g_ascii_isspace (*start) || ((*start) == '\n'));
         start++)
        ;

    memmove (string, start, strlen ((gchar *) start) + 1);
    return string;
}

/**
 * btd_get_state_dir:
 *
 * Returns: (transfer full): the state dir for Btrfsd.
 */
gchar *
btd_get_state_dir (void)
{
    gchar *path = g_build_filename (SHAREDSTATEDIR, "btrfsd", NULL);
    g_mkdir_with_parents (path, 0755);
    return path;
}

/**
 * btd_get_resource_data:
 * @resource_path: Path to the resource.
 *
 * Load data from an internal resource.
 *
 * returns: The resource bytes, or %NULL on error.
 **/
GBytes *
btd_get_resource_data (const gchar *resource_path)
{
    return g_resource_lookup_data (btd_get_resource (),
                                   resource_path,
                                   G_RESOURCE_LOOKUP_FLAGS_NONE,
                                   NULL);
}

/**
 * btd_user_is_root:
 *
 * Check if the current user has superuser permissions.
 */
gboolean
btd_user_is_root (void)
{
    uid_t vuid;
    vuid = getuid ();
    return (vuid == ((uid_t) 0));
}

/**
 * btd_parse_duration_string:
 * @str: The string to parse.
 *
 * Returns: The duration in seconds, or 0 on error.
 */
gulong
btd_parse_duration_string (const gchar *str)
{
    gchar suffix;
    gint64 value;
    guint str_len;
    gulong multiplier = 1;

    if (btd_is_empty (str))
        return 0;
    str_len = strlen (str);
    if (str_len < 1)
        return 0;

    suffix = str[str_len - 1];
    value = g_ascii_strtoll (str, NULL, 10);
    if (value <= 0)
        return 0;

    switch (suffix) {
    case 'h':
        multiplier = SECONDS_IN_AN_HOUR;
        break;
    case 'd':
        multiplier = SECONDS_IN_A_DAY;
        break;
    case 'w':
        multiplier = SECONDS_IN_A_WEEK;
        break;
    case 'M':
        multiplier = SECONDS_IN_A_MONTH;
        break;

    default:
        /* the last character not being a digit is an error */
        if (!g_ascii_isdigit (suffix))
            return 0;

        /* if no suffix, default to hours */
        multiplier = SECONDS_IN_AN_HOUR;
        break;
    }

    return value * multiplier;
}

/**
 * btd_render_template:
 * @template: the template to render
 *
 * Formats a template string to replace all placeholder key with the
 * given values.
 *
 * Returns: (transfer full): Template text with variables replaced.
 **/
gchar *
btd_render_template (const gchar *template, const gchar *key1, ...)
{
    va_list args;
    const gchar *cur_key;
    const gchar *cur_val;
    g_auto(GStrv) parts = NULL;
    g_autoptr(GPtrArray) vars = g_ptr_array_new ();

    if (template == NULL)
        return NULL;
    if (key1 == NULL)
        return g_strdup (template);

    /* prepare variable list */
    va_start (args, key1);
    cur_key = key1;
    while (cur_key != NULL) {
        cur_val = va_arg (args, gchar *);
        if (cur_val == NULL)
            cur_val = "";
        g_ptr_array_add (vars, (gchar *) cur_key);
        g_ptr_array_add (vars, (gchar *) cur_val);

        cur_key = va_arg (args, gchar *);
    }
    va_end (args);

    /* sanity check */
    g_return_val_if_fail (vars->len % 2 == 0, NULL);

    /* replace variables */
    parts = g_strsplit (template, "{{", -1);
    for (guint i = 0; parts[i] != NULL; i++) {
        gboolean replaced = FALSE;

        for (guint j = 0; j < vars->len; j += 2) {
            g_autofree gchar *tmp2 = NULL;
            g_autofree gchar *tmp = g_strconcat (g_ptr_array_index (vars, j), "}}", NULL);
            if (!g_str_has_prefix (parts[i], tmp))
                continue;

            /* replace string */
            tmp2 = parts[i];
            parts[i] = parts[i] + strlen (tmp);
            parts[i] = g_strconcat (g_ptr_array_index (vars, j + 1), parts[i], NULL);
            replaced = TRUE;
            break;
        }

        if (!replaced && (i != 0)) {
            g_autofree gchar *tmp = NULL;

            /* keep the placeholder in place */
            tmp = parts[i];
            parts[i] = g_strconcat ("{{", parts[i], NULL);
        }
    }

    return g_strjoinv ("", parts);
}

/**
 * btd_path_to_filename:
 * @path: The path to convert.
 *
 * Returns: (transfer full): A filename representing the path.
 */
gchar *
btd_path_to_filename (const gchar *path)
{
    GString *str;
    guint short_hash;
    g_autofree gchar *canonical_path = NULL;

    canonical_path = g_canonicalize_filename (path, "/");
    str = g_string_new (canonical_path);
    short_hash = g_str_hash (canonical_path);

    if (g_str_has_prefix (str->str, "/"))
        g_string_erase (str, 0, 1);
    if (str->len == 0) {
        /* we hit the root path / */
        g_string_free (str, TRUE);
        return g_strdup ("-");
    }
    if (g_str_has_prefix (str->str, "."))
        g_string_prepend_c (str, '_');

    g_string_replace (str, "/", "-", 0);
    g_string_replace (str, "\\", "-", 0);

    /* append the hash value to the filename, for some extra uniqueness for corner cases */
    g_string_append_printf (str, "_%u", short_hash);

    return g_string_free (str, FALSE);
}

/**
 * btd_humanize_time:
 * @seconds: The time in seconds.
 *
 * Convert a time in seconds into a human-readable string.
 *
 * Returns: (transfer full): The human-readable string containing the time.
 */
gchar *
btd_humanize_time (gint64 seconds)
{
    if (seconds < 60)
        return g_strdup_printf ("%" G_GINT64_FORMAT " %s",
                                seconds,
                                seconds == 1 ? "second" : "seconds");

    if (seconds < SECONDS_IN_AN_HOUR) {
        if (seconds % 60 == 0)
            return g_strdup_printf ("%" G_GINT64_FORMAT " %s",
                                    seconds / 60,
                                    (seconds / 60) == 1 ? "minute" : "minutes");
        else
            return g_strdup_printf ("%" G_GINT64_FORMAT " %s %" G_GINT64_FORMAT " %s",
                                    seconds / 60,
                                    (seconds / 60) == 1 ? "minute" : "minutes",
                                    seconds % 60,
                                    (seconds % 60) == 1 ? "second" : "seconds");
    }

    if (seconds < SECONDS_IN_A_DAY) {
        if ((seconds % SECONDS_IN_AN_HOUR) / 60 == 0)
            return g_strdup_printf ("%" G_GINT64_FORMAT " %s",
                                    seconds / SECONDS_IN_AN_HOUR,
                                    (seconds / SECONDS_IN_AN_HOUR) == 1 ? "hour" : "hours");
        else
            return g_strdup_printf ("%" G_GINT64_FORMAT " %s %" G_GINT64_FORMAT " %s",
                                    seconds / SECONDS_IN_AN_HOUR,
                                    (seconds / SECONDS_IN_AN_HOUR) == 1 ? "hour" : "hours",
                                    (seconds % SECONDS_IN_AN_HOUR) / 60,
                                    ((seconds % SECONDS_IN_AN_HOUR) / 60) == 1 ? "minute"
                                                                               : "minutes");
    }

    if (seconds < SECONDS_IN_A_MONTH) {
        if ((seconds % SECONDS_IN_A_DAY) / SECONDS_IN_AN_HOUR == 0)
            return g_strdup_printf ("%" G_GINT64_FORMAT " %s",
                                    seconds / SECONDS_IN_A_DAY,
                                    (seconds / SECONDS_IN_A_DAY) == 1 ? "day" : "days");
        else
            return g_strdup_printf (
                "%" G_GINT64_FORMAT " %s %" G_GINT64_FORMAT " %s",
                seconds / SECONDS_IN_A_DAY,
                (seconds / SECONDS_IN_A_DAY) == 1 ? "day" : "days",
                (seconds % SECONDS_IN_A_DAY) / SECONDS_IN_AN_HOUR,
                ((seconds % SECONDS_IN_A_DAY) / SECONDS_IN_AN_HOUR) == 1 ? "hour" : "hours");
    }

    if ((seconds % SECONDS_IN_A_MONTH) / SECONDS_IN_A_DAY == 0)
        return g_strdup_printf ("%" G_GINT64_FORMAT " %s",
                                seconds / SECONDS_IN_A_MONTH,
                                (seconds / SECONDS_IN_A_MONTH) == 1 ? "month" : "months");
    else
        return g_strdup_printf ("%" G_GINT64_FORMAT " %s %" G_GINT64_FORMAT " %s",
                                seconds / SECONDS_IN_A_MONTH,
                                (seconds / SECONDS_IN_A_MONTH) == 1 ? "month" : "months",
                                (seconds % SECONDS_IN_A_MONTH) / SECONDS_IN_A_DAY,
                                ((seconds % SECONDS_IN_A_MONTH) / SECONDS_IN_A_DAY) == 1 ? "day"
                                                                                         : "days");
}

/**
 * btd_machine_is_on_battery:
 *
 * Check if the system is currently running on battery power.
 * We will ask UPower first, and fall back to reading data from /sys
 * if that fails.
 *
 * Returns: %TRUE if system is on battery.
 */
gboolean
btd_machine_is_on_battery ()
{
    g_autoptr(GDBusConnection) connection = NULL;
    g_autoptr(GVariant) result = NULL;
    g_autoptr(GError) dbus_error = NULL;
    guint32 state;

    connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &dbus_error);
    if (connection != NULL) {
        result = g_dbus_connection_call_sync (
            connection,
            "org.freedesktop.UPower",
            "/org/freedesktop/UPower/devices/battery_BAT0",
            "org.freedesktop.DBus.Properties",
            "Get",
            g_variant_new ("(ss)", "org.freedesktop.UPower.Device", "State"),
            G_VARIANT_TYPE ("(v)"),
            G_DBUS_CALL_FLAGS_NONE,
            -1,
            NULL,
            &dbus_error);
    }

    if (result == NULL) {
        g_autofree gchar *contents = NULL;
        g_autoptr(GError) file_error = NULL;

        /* we failed to red battery information from UPower - let's try using /sys directly */
        if (!g_file_get_contents ("/sys/class/power_supply/BAT0/status",
                                  &contents,
                                  NULL,
                                  &file_error)) {
            g_debug ("Unable to read battery status (UPower failed: %s, Reading BAT0 failed: %s). "
                     "The system may not have a battery.",
                     dbus_error->message,
                     file_error->message);
            /* this is not an error, the machine may not have a battery */
            return FALSE;
        }

        /* we're on battery if we're discharging */
        return g_str_has_prefix (g_strstrip (contents), "Discharging");
    }

    /* extract battery state from D-Bus result */
    g_variant_get (result, "(v)", &result);
    state = g_variant_get_uint32 (result);

    g_variant_unref (result);
    g_object_unref (connection);

    /* 2 is the value for "discharging" in upower */
    return state == 2;
}
