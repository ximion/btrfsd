/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define SECONDS_IN_AN_HOUR (60 * 60)
#define SECONDS_IN_A_DAY   (24 * SECONDS_IN_AN_HOUR)
#define SECONDS_IN_A_WEEK  (7 * SECONDS_IN_A_DAY)
/* we assume an average month has approximately 30.44 days here */
#define SECONDS_IN_A_MONTH ((int) (30.44 * SECONDS_IN_A_DAY))

/**
 * btd_str_equal0:
 * Returns TRUE if strings are equal, ignoring NULL strings.
 * This is a convenience wrapper around g_strcmp0
 */
#define btd_str_equal0(str1, str2) (g_strcmp0 ((gchar *) str1, (gchar *) str2) == 0)

gboolean btd_is_empty (const gchar *str);
gchar   *btd_strstripnl (gchar *string);

gchar   *btd_get_state_dir (void);
GBytes  *btd_get_resource_data (const gchar *resource_path);

gulong   btd_parse_duration_string (const gchar *str);

gchar   *btd_render_template (const gchar *template, const gchar *key1, ...) G_GNUC_NULL_TERMINATED;

gchar   *btd_path_to_filename (const gchar *path);

gchar   *btd_humanize_time (gint64 seconds);

G_END_DECLS
