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
