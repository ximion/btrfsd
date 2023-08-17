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
#include "btd-mount-record.h"

typedef struct {
    gboolean loaded;
    GPtrArray *mountpoints;
    GKeyFile *config;
    gchar *state_dir;

    gulong default_intervals[BTD_BTRFS_ACTION_LAST];
} BtdSchedulerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (BtdScheduler, btd_scheduler, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (btd_scheduler_get_instance_private (o))

static void
btd_scheduler_init (BtdScheduler *self)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    gulong seconds_in_month;

    priv->config = g_key_file_new ();
    priv->state_dir = btd_get_state_dir ();

    seconds_in_month = btd_parse_duration_string ("1M");
    for (guint i = 0; i < BTD_BTRFS_ACTION_LAST; i++)
        priv->default_intervals[i] = seconds_in_month;
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

static gchar *
btd_get_interval_key (BtdBtrfsAction action_kind)
{
    return g_strconcat (btd_btrfs_action_to_string (action_kind), "_interval", NULL);
}

static gulong
btd_scheduler_get_config_duration_str (BtdScheduler *self,
                                       const gchar *group,
                                       const gchar *key,
                                       const gchar *default_value)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *value = NULL;

    value = g_key_file_get_string (priv->config, group, key, NULL);
    if (value == NULL)
        return btd_parse_duration_string (default_value);
    return btd_parse_duration_string (value);
}

static gulong
btd_scheduler_get_config_duration (BtdScheduler *self,
                                   const gchar *group,
                                   const gchar *key,
                                   gulong default_value)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *value = NULL;

    value = g_key_file_get_string (priv->config, group, key, NULL);
    if (value == NULL)
        return default_value;
    return btd_parse_duration_string (value);
}

static gulong
btd_scheduler_get_config_duration_for_action (BtdScheduler *self,
                                              BtdBtrfsMount *bmount,
                                              BtdBtrfsAction action_kind)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *key_name = NULL;

    key_name = btd_get_interval_key (action_kind);
    return btd_scheduler_get_config_duration (self,
                                              btd_btrfs_mount_get_mountpoint (bmount),
                                              key_name,
                                              priv->default_intervals[action_kind]);
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

    priv->default_intervals[BTD_BTRFS_ACTION_SCRUB] = btd_scheduler_get_config_duration_str (
        self,
        "default",
        btd_get_interval_key (BTD_BTRFS_ACTION_SCRUB),
        "1M");
    priv->default_intervals[BTD_BTRFS_ACTION_STATS] = btd_scheduler_get_config_duration_str (
        self,
        "default",
        btd_get_interval_key (BTD_BTRFS_ACTION_STATS),
        "1h");
    priv->default_intervals[BTD_BTRFS_ACTION_BALANCE] = btd_scheduler_get_config_duration_str (
        self,
        "default",
        btd_get_interval_key (BTD_BTRFS_ACTION_BALANCE),
        "6M");

    priv->loaded = TRUE;
    return TRUE;
}

static gboolean
btd_scheduler_run_stats (BtdScheduler *self, BtdBtrfsMount *bmount)
{
    return TRUE;
}

static gboolean
btd_scheduler_run_for_mount (BtdScheduler *self, BtdBtrfsMount *bmount)
{
    g_autoptr(BtdMountRecord) record = NULL;
    g_autoptr(GError) error = NULL;
    gint64 current_time;
    gint64 last_time;
    gulong interval_time;

    record = btd_mount_record_new (btd_btrfs_mount_get_mountpoint (bmount));
    if (!btd_mount_record_load (record, &error)) {
        g_warning ("Unable to load record for mount '%s': %s",
                   btd_btrfs_mount_get_mountpoint (bmount),
                   error->message);
        g_clear_error (&error);
    }

    /* current time */
    current_time = (gint64) time (NULL);

    /* Stats Action */
    interval_time = btd_scheduler_get_config_duration_for_action (self,
                                                                  bmount,
                                                                  BTD_BTRFS_ACTION_STATS);
    last_time = btd_mount_record_get_last_action_time (record, BTD_BTRFS_ACTION_STATS);
    if (current_time - last_time > interval_time) {
        btd_scheduler_run_stats (self, bmount);
        btd_mount_record_set_last_action_time_now (record, BTD_BTRFS_ACTION_STATS);
    }

    /* save record & finish */
    if (!btd_mount_record_save (record, &error)) {
        g_warning ("Unable to save state record for mount '%s': %s",
                   btd_btrfs_mount_get_mountpoint (bmount),
                   error->message);
        g_clear_error (&error);
    }
    return TRUE;
}

/**
 * btd_scheduler_run:
 * @self: An instance of #BtdScheduler
 * @error: A #GError
 *
 * Run any actions that are pending.
 *
 * Returns: %TRUE on success.
 */
gboolean
btd_scheduler_run (BtdScheduler *self, GError **error)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);

    /* load configuration in case we haven't loaded it yet */
    if (!priv->loaded) {
        if (!btd_scheduler_load (self, error))
            return FALSE;
    }

    /* check if there is anything for us to do */
    if (priv->mountpoints->len == 0) {
        g_debug ("No mounted Btrfs filesystems found.");
        return TRUE;
    }

    /* run tasks */
    for (guint i = 0; i < priv->mountpoints->len; i++) {
        BtdBtrfsMount *bmount = g_ptr_array_index (priv->mountpoints, i);
        btd_scheduler_run_for_mount (self, bmount);
    }

    return TRUE;
}
