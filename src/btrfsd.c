/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "btd-scheduler.h"
#include "btd-btrfs-mount.h"

int
main (int argc, char **argv)
{
    g_autoptr(GOptionContext) option_context = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(BtdScheduler) scheduler = NULL;
    gboolean ret;
    gboolean verbose = FALSE;
    gboolean show_version = FALSE;
    gboolean show_status = FALSE;

    const GOptionEntry options[] = {
        { "verbose",
          'v',
          0,
          G_OPTION_ARG_NONE,
          &verbose,
          "Show extra debugging information",
          NULL },
        { "version", '\0', 0, G_OPTION_ARG_NONE, &show_version, "Show the program version.", NULL },
        { "status",
          '\0',
          0,
          G_OPTION_ARG_NONE,
          &show_status,
          "Display some short status information.",
          NULL },
        { NULL }
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
        g_print ("Btrfsd version: %s\n", PACKAGE_VERSION);
        return EXIT_SUCCESS;
    }

    scheduler = btd_scheduler_new ();
    if (!btd_scheduler_load (scheduler, &error)) {
        g_printerr ("Failed to initialize: %s\n", error->message);
        return EXIT_FAILURE;
    }

    if (show_status) {
        /* display status information */
        if (btd_scheduler_print_status (scheduler))
            return EXIT_SUCCESS;
        return EXIT_FAILURE;
    }

    /* run all scheduled actions */
    if (!btd_scheduler_run (scheduler, &error)) {
        g_printerr ("Failed to run: %s\n", error->message);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
