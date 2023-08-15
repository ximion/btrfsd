/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

int
main (int argc, char **argv)
{
    g_autoptr(GOptionContext) option_context = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ret;
    gboolean verbose = FALSE;
    gboolean show_version = FALSE;

    const GOptionEntry options[] = {
        {"verbose",
         'v', 0,
         G_OPTION_ARG_NONE, &verbose,
         "Show extra debugging information", NULL },
        { "version",        '\0', 0, G_OPTION_ARG_NONE, &show_version, "Show the program version.", NULL },
        { NULL}
    };

    setlocale (LC_ALL, "");
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    option_context = g_option_context_new ("Btrfs maintenance helper.");

    g_option_context_add_main_entries (option_context, options, NULL);
    ret = g_option_context_parse (option_context, &argc, &argv, &error);
    if (!ret) {
        g_print ("%s: %s\n", "Failed to parse arguments", error->message);
        return EXIT_FAILURE;
    }

    if (verbose)
        g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

    if (show_version) {
        g_print ("Btrfsd version: %s", PACKAGE_VERSION);
        return EXIT_SUCCESS;
    }

    /* TODO */

    return EXIT_SUCCESS;
}
