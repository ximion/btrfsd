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
