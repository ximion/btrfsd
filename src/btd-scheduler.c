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
#include "btd-mailer.h"
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

typedef gboolean (*BtdActionFunction) (BtdScheduler *, BtdBtrfsMount *, BtdMountRecord *);

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

static gchar *
btd_scheduler_get_config_value (BtdScheduler *self,
                                BtdBtrfsMount *bmount,
                                const gchar *key,
                                const gchar *default_value)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *value = NULL;

    if (bmount == NULL) {
        value = g_key_file_get_string (priv->config, "general", key, NULL);
        if (value == NULL)
            return g_strdup (default_value);
        return value;
    }

    value = g_key_file_get_string (priv->config,
                                   btd_btrfs_mount_get_mountpoint (bmount),
                                   key,
                                   NULL);
    if (value == NULL) {
        /* check generic section */
        value = g_key_file_get_string (priv->config, "general", key, NULL);
    }

    if (value == NULL)
        return g_strdup (default_value);
    return value;
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
btd_scheduler_run_stats (BtdScheduler *self, BtdBtrfsMount *bmount, BtdMountRecord *record)
{
    g_autofree gchar *mail_address = NULL;
    g_autofree gchar *issue_report = NULL;
    gboolean errors_found = FALSE;
    g_autoptr(GError) error = NULL;

    g_debug ("Reading stats for %s", btd_btrfs_mount_get_mountpoint (bmount));

    mail_address = btd_scheduler_get_config_value (self, bmount, "mail_address", NULL);
    if (!btd_btrfs_mount_read_error_stats (bmount, &issue_report, &errors_found, &error)) {
        /* only log the error for now */
        g_printerr ("Failed to query btrfs issue statistics for '%s': %s\n",
                    btd_btrfs_mount_get_mountpoint (bmount),
                    error->message);
        return FALSE;
    }

    /* we have nothing more to do if no errors were found */
    if (!errors_found)
        return TRUE;

    if (mail_address == NULL) {
        g_print ("Errors detected on filesystem '%s'!\n", btd_btrfs_mount_get_mountpoint (bmount));
        return TRUE;
    }

    {
        g_autoptr(GBytes) template_bytes = NULL;
        g_autofree gchar *formatted_time = NULL;
        g_autofree gchar *mail_body = NULL;
        g_autofree gchar *fs_usage = NULL;
        time_t current_time;
        g_autoptr(GDateTime) dt_now = g_date_time_new_now_local ();

        current_time = time (NULL);
        if (current_time - btd_mount_record_get_value_int (record, "mails", "issue_mail_sent", 0) <
            SECONDS_IN_A_DAY) {
            g_debug ("Issue email for '%s' already sent today, will try again tomorrow if the "
                     "issue persists.",
                     btd_btrfs_mount_get_mountpoint (bmount));
            return TRUE;
        }

        formatted_time = g_date_time_format (dt_now, "%Y-%m-%d %H:%M:%S");

        template_bytes = btd_get_resource_data ("/btrfsd/error-mail.tmpl");
        if (template_bytes == NULL) {
            g_critical ("Failed to find error-mail template data. This is a bug.");
            return FALSE;
        }

        fs_usage = btd_btrfs_mount_read_usage (bmount, NULL);
        if (fs_usage == NULL)
            fs_usage = g_strdup ("⚠ Failed to read usage data.");
        mail_body = btd_render_template (g_bytes_get_data (template_bytes, NULL),
                                         "date_time",
                                         formatted_time,
                                         "hostname",
                                         g_get_host_name (),
                                         "mountpoint",
                                         btd_btrfs_mount_get_mountpoint (bmount),
                                         "issue_report",
                                         issue_report,
                                         "fs_usage",
                                         fs_usage,
                                         NULL);

        if (!btd_send_email (mail_address, mail_body, &error)) {
            g_warning ("Failed to send issue mail: %s", error->message);
            return FALSE;
        }

        /* we have sent the mail, record that fact so we don't spam messages too frequently */
        btd_mount_record_set_value_int (record, "mails", "issue_mail_sent", current_time);
    }

    return TRUE;
}

static gboolean
btd_scheduler_run_scrub (BtdScheduler *self, BtdBtrfsMount *bmount, BtdMountRecord *record)
{
    g_autoptr(GError) error = NULL;

    g_debug ("Running scrub on filesystem %s", btd_btrfs_mount_get_mountpoint (bmount));
    if (!btd_btrfs_mount_scrub (bmount, &error)) {
        g_warning ("Scrub on %s failed: %s",
                   btd_btrfs_mount_get_mountpoint (bmount),
                   error->message);
        return FALSE;
    }

    return TRUE;
}

static gboolean
btd_scheduler_run_balance (BtdScheduler *self, BtdBtrfsMount *bmount, BtdMountRecord *record)
{
    /* TODO */
    return TRUE;
}

static gboolean
btd_scheduler_run_for_mount (BtdScheduler *self, BtdBtrfsMount *bmount)
{
    g_autoptr(BtdMountRecord) record = NULL;
    g_autoptr(GError) error = NULL;
    time_t current_time;
    gint64 last_time;
    gulong interval_time;
    struct {
        BtdBtrfsAction action;
        BtdActionFunction func;
    } action_fn[] = {
        { BTD_BTRFS_ACTION_STATS, btd_scheduler_run_stats },
        { BTD_BTRFS_ACTION_SCRUB, btd_scheduler_run_scrub },
        { BTD_BTRFS_ACTION_BALANCE, btd_scheduler_run_balance },

        { BTD_BTRFS_ACTION_UNKNOWN, NULL },
    };

    record = btd_mount_record_new (btd_btrfs_mount_get_mountpoint (bmount));
    if (!btd_mount_record_load (record, &error)) {
        g_warning ("Unable to load record for mount '%s': %s",
                   btd_btrfs_mount_get_mountpoint (bmount),
                   error->message);
        g_clear_error (&error);
    }

    /* current time */
    current_time = time (NULL);

    /* run all actions */
    for (guint i = 0; action_fn[i].func != NULL; i++) {
        interval_time = btd_scheduler_get_config_duration_for_action (self,
                                                                      bmount,
                                                                      action_fn[i].action);
        last_time = btd_mount_record_get_last_action_time (record, action_fn[i].action);
        if (current_time - last_time > interval_time) {
            action_fn[i].func (self, bmount, record);
            btd_mount_record_set_last_action_time_now (record, action_fn[i].action);
        }
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
