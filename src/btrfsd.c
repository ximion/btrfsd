/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "btd-btrfs-mount.h"

/**
 * btd_show_status:
 *
 * Display some status information as console output.
 */
static gint
btd_show_status (void)
{
    g_autoptr(GPtrArray) mountpoints = NULL;
    g_autoptr(GError) error = NULL;

    mountpoints = btd_find_mounted_btrfs_filesystems (&error);
    if (mountpoints == NULL) {
        g_printerr ("Unable to find mounted Btrfs filesystems: %s\n", error->message);
        return EXIT_FAILURE;
    }

    if (mountpoints->len == 0) {
        g_print ("No mounted Btrfs filesystems found.\n");
        return EXIT_SUCCESS;
    }

    g_print ("Mounted Btrfs filesystems:\n");
    for (guint i = 0; i < mountpoints->len; i++) {
        BtdBtrfsMount *bmount = g_ptr_array_index (mountpoints, i);
        g_print ("  • %s  →  %s\n",
                 btd_btrfs_mount_get_mountpoint (bmount),
                 btd_btrfs_mount_get_device_name (bmount));
    }

    return EXIT_SUCCESS;
}

int
main (int argc, char **argv)
{
    g_autoptr(GOptionContext) option_context = NULL;
    g_autoptr(GError) error = NULL;
    gboolean ret;
    gboolean verbose = FALSE;
    gboolean show_version = FALSE;
    gboolean show_status = FALSE;

    const GOptionEntry options[] = {
        {"verbose",
         'v', 0,
         G_OPTION_ARG_NONE, &verbose,
         "Show extra debugging information", NULL },
        { "version",        '\0', 0, G_OPTION_ARG_NONE, &show_version, "Show the program version.", NULL },
        { "status",
         '\0', 0,
         G_OPTION_ARG_NONE, &show_status,
         "Display some short status information.", NULL },
        { NULL    }
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

    if (show_status) {
        /* display status information */
        return btd_show_status ();
    }

    /* TODO */

    return EXIT_SUCCESS;
}
