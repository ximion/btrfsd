/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * btd_str_equal0:
 * Returns TRUE if strings are equal, ignoring NULL strings.
 * This is a convenience wrapper around g_strcmp0
 */
#define btd_str_equal0(str1, str2) (g_strcmp0 ((gchar *) str1, (gchar *) str2) == 0)

gboolean btd_is_empty (const gchar *str);

G_END_DECLS
