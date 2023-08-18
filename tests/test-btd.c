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
    g_assert_cmpint (btd_parse_duration_string ("1M"), ==, 2630016);
    g_assert_cmpint (btd_parse_duration_string ("3M"), ==, 2630016 * 3);
    g_assert_cmpint (btd_parse_duration_string ("notvalid"), ==, 0);
    g_assert_cmpint (btd_parse_duration_string ("2u"), ==, 0);
}

/**
 * test_render_template:
 */
static void
test_render_template (void)
{
    const gchar
        *template1 = "This is a {{key1}} template\n"
                     "All strings need to be {{action}} correctly for the {{test_name}} to pass.";
    const gchar *
        result1 = "This is a good template\n"
                  "All strings need to be rendered correctly for the render_template test to pass.";
    g_autofree gchar *tmp = NULL;

    tmp = btd_render_template (template1,
                               "key1",
                               "good",
                               "action",
                               "rendered",
                               "test_name",
                               "render_template test",
                               NULL);
    g_assert_cmpstr (tmp, ==, result1);
}

/**
 * test_path_escape:
 */
static void
test_path_escape (void)
{
    gchar *tmp;

    tmp = btd_path_to_filename ("/");
    g_assert_cmpstr (tmp, ==, "-");
    g_free (tmp);

    tmp = btd_path_to_filename ("");
    g_assert_cmpstr (tmp, ==, "-");
    g_free (tmp);

    tmp = btd_path_to_filename ("/this/is/a path with/spaces/.txt");
    g_assert_cmpstr (tmp, ==, "this-is-a path with-spaces-.txt_4128569403");
    g_free (tmp);

    tmp = btd_path_to_filename ("..");
    g_assert_cmpstr (tmp, ==, "-");
    g_free (tmp);

    tmp = btd_path_to_filename ("/../../.");
    g_assert_cmpstr (tmp, ==, "-");
    g_free (tmp);

    tmp = btd_path_to_filename ("/a/cräzü/path----/x/../txt");
    g_assert_cmpstr (tmp, ==, "a-cräzü-path-----txt_3474729208");
    g_free (tmp);

    tmp = btd_path_to_filename ("/a-b/c");
    g_assert_cmpstr (tmp, ==, "a-b-c_2088179606");
    g_free (tmp);

    tmp = btd_path_to_filename ("/a/b/c");
    g_assert_cmpstr (tmp, ==, "a-b-c_2088251480");
    g_free (tmp);
}

/**
 * test_humanize_time:
 */
static void
test_humanize_time (void)
{
    g_autofree gchar *result = NULL;

    result = btd_humanize_time (5);
    g_assert_cmpstr (result, ==, "5 seconds");
    g_clear_pointer (&result, g_free);

    result = btd_humanize_time (1);
    g_assert_cmpstr (result, ==, "1 second");
    g_clear_pointer (&result, g_free);

    result = btd_humanize_time (70);
    g_assert_cmpstr (result, ==, "1 minute 10 seconds");
    g_clear_pointer (&result, g_free);

    result = btd_humanize_time (120);
    g_assert_cmpstr (result, ==, "2 minutes");
    g_clear_pointer (&result, g_free);

    result = btd_humanize_time (3600);
    g_assert_cmpstr (result, ==, "1 hour");
    g_clear_pointer (&result, g_free);

    result = btd_humanize_time (3660);
    g_assert_cmpstr (result, ==, "1 hour 1 minute");
    g_clear_pointer (&result, g_free);

    result = btd_humanize_time (SECONDS_IN_A_DAY);
    g_assert_cmpstr (result, ==, "1 day");
    g_clear_pointer (&result, g_free);

    result = btd_humanize_time (SECONDS_IN_A_DAY + SECONDS_IN_AN_HOUR);
    g_assert_cmpstr (result, ==, "1 day 1 hour");
    g_clear_pointer (&result, g_free);

    result = btd_humanize_time (SECONDS_IN_A_MONTH);
    g_assert_cmpstr (result, ==, "1 month");
    g_clear_pointer (&result, g_free);

    result = btd_humanize_time (SECONDS_IN_A_MONTH + SECONDS_IN_A_DAY);
    g_assert_cmpstr (result, ==, "1 month 1 day");
    g_clear_pointer (&result, g_free);
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
    g_test_add_func ("/Btrfsd/Misc/RenderTemplate", test_render_template);
    g_test_add_func ("/Btrfsd/Misc/PathEscape", test_path_escape);
    g_test_add_func ("/Btrfsd/Misc/HumanizeTime", test_humanize_time);

    ret = g_test_run ();
    return ret;
}
