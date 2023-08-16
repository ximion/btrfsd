/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <glib.h>

#include "btd-utils.h"

/**
 * test_duration_parser:
 */
static void
test_duration_parser (void)
{
    g_assert_cmpint (btd_parse_duration_string ("1h"), ==, 3600);
    g_assert_cmpint (btd_parse_duration_string ("2h"), ==, 3600 * 2);
    g_assert_cmpint (btd_parse_duration_string ("3"), ==, 3600 * 3);
    g_assert_cmpint (btd_parse_duration_string ("1d"), ==, 86400);
    g_assert_cmpint (btd_parse_duration_string ("4d"), ==, 86400 * 4);
    g_assert_cmpint (btd_parse_duration_string ("1w"), ==, 604800);
    g_assert_cmpint (btd_parse_duration_string ("4w"), ==, 604800 * 4);
    g_assert_cmpint (btd_parse_duration_string ("1m"), ==, 2630016);
    g_assert_cmpint (btd_parse_duration_string ("3m"), ==, 2630016 * 3);
    g_assert_cmpint (btd_parse_duration_string ("notvalid"), ==, 0);
    g_assert_cmpint (btd_parse_duration_string ("2u"), ==, 0);
}

int
main (int argc, char **argv)
{
    int ret;

    if (argc == 0) {
        g_error ("No test directory specified!");
        return 1;
    }

    g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
    g_test_init (&argc, &argv, NULL);

    /* only critical and error are fatal */
    g_log_set_fatal_mask (NULL, G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

    g_test_add_func ("/Btrfsd/Misc/DurationParser", test_duration_parser);

    ret = g_test_run ();
    return ret;
}
