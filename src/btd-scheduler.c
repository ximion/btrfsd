/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:btd-scheduler
 * @short_description: Scheduler for Btrfs maintenance actions.
 *
 * Schedules various maintenance actions according to the user's preferences.
 */

#include "config.h"
#include "btd-scheduler.h"

#include "btd-utils.h"
#include "btd-btrfs-mount.h"

typedef struct {
    gboolean loaded;
    GPtrArray *mountpoints;
    GKeyFile *config;

    gchar *state_dir;
} BtdSchedulerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (BtdScheduler, btd_scheduler, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (btd_scheduler_get_instance_private (o))

static void
btd_scheduler_init (BtdScheduler *self)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);

    priv->config = g_key_file_new ();
    priv->state_dir = g_build_filename (LOCALSTATEDIR, "lib", "btrfsd", NULL);
}

static void
btd_scheduler_finalize (GObject *object)
{
    BtdScheduler *self = BTD_SCHEDULER (object);
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);

    g_free (priv->state_dir);
    g_key_file_unref (priv->config);
    if (priv->mountpoints != NULL)
        g_ptr_array_unref (priv->mountpoints);

    G_OBJECT_CLASS (btd_scheduler_parent_class)->finalize (object);
}

static void
btd_scheduler_class_init (BtdSchedulerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = btd_scheduler_finalize;
}

/**
 * btd_scheduler_new:
 *
 * Creates a new #BtdScheduler.
 *
 * Returns: (transfer full): a #BtdScheduler
 */
BtdScheduler *
btd_scheduler_new ()
{
    BtdScheduler *self;
    self = g_object_new (BTD_TYPE_SCHEDULER, NULL);
    return BTD_SCHEDULER (self);
}

/**
 * btd_scheduler_load:
 * @self: An instance of #BtdScheduler
 * @error: A #GError
 *
 * Load scheduler data and set up data structures.
 *
 * Returns: %TRUE on success.
 */
gboolean
btd_scheduler_load (BtdScheduler *self, GError **error)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    const gchar *config_fname = SYSCONFDIR "/btrfsd/settings.conf";
    GError *tmp_error = NULL;

    if (priv->loaded) {
        g_set_error_literal (error,
                             BTD_BTRFS_ERROR,
                             BTD_BTRFS_ERROR_FAILED,
                             "Tried to initialize already initialized scheduler.");
        return FALSE;
    }

    if (priv->mountpoints != NULL)
        g_ptr_array_unref (priv->mountpoints);
    priv->mountpoints = btd_find_mounted_btrfs_filesystems (error);
    if (priv->mountpoints == NULL)
        return FALSE;

    if (g_file_test (config_fname, G_FILE_TEST_EXISTS)) {
        if (!g_key_file_load_from_file (priv->config, config_fname, G_KEY_FILE_NONE, &tmp_error)) {
            g_propagate_prefixed_error (error, tmp_error, "Failed to load configuration:");
            return FALSE;
        }
    }

    priv->loaded = TRUE;
    return TRUE;
}
