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
