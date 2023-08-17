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

#define SECONDS_IN_AN_HOUR (60 * 60)
#define SECONDS_IN_A_DAY   (24 * SECONDS_IN_AN_HOUR)
#define SECONDS_IN_A_WEEK  (7 * SECONDS_IN_A_DAY)
/* we assume an average month has approximately 30.44 days here */
#define SECONDS_IN_A_MONTH ((int) (30.44 * SECONDS_IN_A_DAY))

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
    g_mkdir_with_parents (path, 0644);
    return path;
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

    str = g_string_new (path);
    if (g_str_has_prefix (str->str, "/"))
        g_string_erase (str, 0, 1);
    if (g_str_has_prefix (str->str, "."))
        g_string_prepend_c (str, '_');
    if (str->len == 0) {
        /* we hit the root path / */
        g_string_free (str, TRUE);
        return g_strdup ("-");
    }

    g_string_replace (str, "/", "-", 0);
    g_string_replace (str, "\\", "-", 0);

    return g_string_free (str, FALSE);
}
