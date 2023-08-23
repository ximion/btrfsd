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
#include "btd-logging.h"
#include "btd-mailer.h"
#include "btd-filesystem.h"
#include "btd-fs-record.h"

typedef struct {
    gboolean loaded;
    GPtrArray *mountpoints;
    GKeyFile *config;
    gchar *state_dir;

    gulong default_intervals[BTD_BTRFS_ACTION_LAST];
} BtdSchedulerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (BtdScheduler, btd_scheduler, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (btd_scheduler_get_instance_private (o))

typedef gboolean (*BtdActionFunction) (BtdScheduler *, BtdFilesystem *, BtdFsRecord *);

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
                                              BtdFilesystem *bfs,
                                              BtdBtrfsAction action_kind)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *key_name = NULL;

    key_name = btd_get_interval_key (action_kind);
    return btd_scheduler_get_config_duration (self,
                                              btd_filesystem_get_mountpoint (bfs),
                                              key_name,
                                              priv->default_intervals[action_kind]);
}

static gchar *
btd_scheduler_get_config_value (BtdScheduler *self,
                                BtdFilesystem *bfs,
                                const gchar *key,
                                const gchar *default_value)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *value = NULL;

    if (bfs == NULL) {
        value = g_key_file_get_string (priv->config, "default", key, NULL);
        if (value == NULL)
            return g_strdup (default_value);
        return g_steal_pointer (&value);
    }

    value = g_key_file_get_string (priv->config, btd_filesystem_get_mountpoint (bfs), key, NULL);
    if (value == NULL) {
        /* check generic section */
        value = g_key_file_get_string (priv->config, "default", key, NULL);
        if (value == NULL)
            return g_strdup (default_value);
    }

    return g_steal_pointer (&value);
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
        btd_debug ("Loaded configuration: %s", config_fname);
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
btd_scheduler_send_error_mail (BtdScheduler *self,
                               BtdFilesystem *bfs,
                               BtdFsRecord *record,
                               gboolean new_errors_found,
                               const gchar *mail_address,
                               const gchar *issue_report)
{
    g_autoptr(GBytes) template_bytes = NULL;
    g_autofree gchar *mail_from = NULL;
    g_autofree gchar *formatted_time = NULL;
    g_autofree gchar *mail_body = NULL;
    g_autofree gchar *fs_usage = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GDateTime) dt_now = g_date_time_new_now_local ();
    time_t current_time;

    current_time = time (NULL);
    if (new_errors_found ||
        current_time - btd_fs_record_get_value_int (record, "messages", "issue_mail_sent", 0) <
            SECONDS_IN_AN_HOUR * 20) {
        g_debug ("Issue email for '%s' already sent today, will try again in 20h if the "
                 "issue persists.",
                 btd_filesystem_get_mountpoint (bfs));
        return TRUE;
    }

    btd_debug ("Sending issue mail to %s", mail_address);
    formatted_time = g_date_time_format (dt_now, "%Y-%m-%d %H:%M:%S");
    mail_from = btd_scheduler_get_config_value (self, bfs, "mail_from", NULL);
    if (mail_from == NULL)
        mail_from = g_strdup ("btrfsd");

    template_bytes = btd_get_resource_data ("/btrfsd/error-mail.tmpl");
    if (template_bytes == NULL) {
        btd_error ("Failed to find error-mail template data. This is a bug.");
        return FALSE;
    }

    fs_usage = btd_filesystem_read_usage (bfs, NULL);
    if (fs_usage == NULL)
        fs_usage = g_strdup ("⚠ Failed to read usage data.");
    mail_body = btd_render_template (g_bytes_get_data (template_bytes, NULL),
                                     "mail_from",
                                     mail_from,
                                     "date_time",
                                     formatted_time,
                                     "hostname",
                                     g_get_host_name (),
                                     "mountpoint",
                                     btd_filesystem_get_mountpoint (bfs),
                                     "issue_report",
                                     issue_report,
                                     "fs_usage",
                                     fs_usage,
                                     NULL);

    if (!btd_send_email (mail_address, mail_body, &error)) {
        btd_warning ("Failed to send issue mail: %s", error->message);
        return FALSE;
    }

    /* we have sent the mail, record that fact so we don't spam messages too frequently */
    btd_fs_record_set_value_int (record, "messages", "issue_mail_sent", current_time);

    return TRUE;
}

static gboolean
btd_scheduler_run_stats (BtdScheduler *self, BtdFilesystem *bfs, BtdFsRecord *record)
{
    g_autofree gchar *mail_address = NULL;
    g_autofree gchar *issue_report = NULL;
    guint64 error_count = 0;
    guint64 prev_error_count = 0;
    g_autoptr(GError) error = NULL;
    time_t current_time;

    g_debug ("Reading stats for %s", btd_filesystem_get_mountpoint (bfs));

    mail_address = btd_scheduler_get_config_value (self, bfs, "mail_address", NULL);
    if (mail_address != NULL)
        mail_address = g_strstrip (mail_address);
    if (!btd_filesystem_read_error_stats (bfs, &issue_report, &error_count, &error)) {
        /* only log the error for now */
        g_printerr ("Failed to query btrfs issue statistics for '%s': %s\n",
                    btd_filesystem_get_mountpoint (bfs),
                    error->message);
        return FALSE;
    }

    /* we have nothing more to do if no errors were found */
    if (error_count == 0) {
        btd_fs_record_set_value_int (record, "errors", "total", 0);
        return TRUE;
    }
    prev_error_count = btd_fs_record_get_value_int (record, "errors", "total", 0);
    btd_fs_record_set_value_int (record, "errors", "total", (gint64) error_count);

    current_time = time (NULL);

    if (error_count > prev_error_count ||
        current_time - btd_fs_record_get_value_int (record, "messages", "broadcast_sent", 0) >
            SECONDS_IN_AN_HOUR * 6) {
        g_autofree gchar *bc_message = NULL;
        /* broadcast message that there are errors to be fixed, do that roughly every 6h */
        bc_message = g_strdup_printf ("⚠ Errors detected on filesystem at %s!\n"
                                      "Please back up your files immediately. You can run "
                                      "`btrfs device stats %s` for issue details.\n",
                                      btd_filesystem_get_mountpoint (bfs),
                                      btd_filesystem_get_mountpoint (bfs));

        btd_broadcast_message (bc_message);
        btd_fs_record_set_value_int (record, "messages", "broadcast_sent", current_time);
    }

    if (btd_is_empty (mail_address)) {
        btd_warning ("Errors detected on filesystem '%s'!\n", btd_filesystem_get_mountpoint (bfs));
        return TRUE;
    }

    return btd_scheduler_send_error_mail (
        self,
        bfs,                            /* filesystem to act on */
        record,                         /* state record for the FS */
        error_count > prev_error_count, /* True if error amount is increasing */
        mail_address,                   /* destination email address */
        issue_report);
}

static gboolean
btd_scheduler_run_scrub (BtdScheduler *self, BtdFilesystem *bfs, BtdFsRecord *record)
{
    g_autoptr(GError) error = NULL;

    g_debug ("Running scrub on filesystem %s", btd_filesystem_get_mountpoint (bfs));
    if (!btd_filesystem_scrub (bfs, &error)) {
        btd_warning ("Scrub on %s failed: %s", btd_filesystem_get_mountpoint (bfs), error->message);
        return FALSE;
    }

    return TRUE;
}

static gboolean
btd_scheduler_run_balance (BtdScheduler *self, BtdFilesystem *bfs, BtdFsRecord *record)
{
    /* TODO */
    return TRUE;
}

static gboolean
btd_scheduler_run_for_mount (BtdScheduler *self, BtdFilesystem *bfs)
{
    g_autoptr(BtdFsRecord) record = NULL;
    g_autoptr(GError) error = NULL;
    gint64 current_time;
    gint64 last_time;
    gulong interval_time;
    struct {
        BtdBtrfsAction action;
        BtdActionFunction func;
        gboolean allow_on_battery;
    } action_fn[] = {
        { BTD_BTRFS_ACTION_STATS, btd_scheduler_run_stats, TRUE },
        { BTD_BTRFS_ACTION_SCRUB, btd_scheduler_run_scrub, FALSE },
        { BTD_BTRFS_ACTION_BALANCE, btd_scheduler_run_balance, FALSE },

        { BTD_BTRFS_ACTION_UNKNOWN, NULL },
    };

    record = btd_fs_record_new (btd_filesystem_get_mountpoint (bfs));
    if (!btd_fs_record_load (record, &error)) {
        btd_warning ("Unable to load record for mount '%s': %s",
                     btd_filesystem_get_mountpoint (bfs),
                     error->message);
        g_clear_error (&error);
    }

    /* current time */
    current_time = (gint64) time (NULL);

    /* run all actions */
    for (guint i = 0; action_fn[i].func != NULL; i++) {
        interval_time = btd_scheduler_get_config_duration_for_action (self,
                                                                      bfs,
                                                                      action_fn[i].action);
        last_time = btd_fs_record_get_last_action_time (record, action_fn[i].action);
        if (current_time - last_time > (gint64) interval_time) {
            /* first check if this action is even allowed to be run if we are on batter power */
            if (!action_fn[i].allow_on_battery && btd_machine_is_on_battery ()) {
                g_debug ("Skipping %s on %s, we are running on battery power.",
                         btd_btrfs_action_to_string (action_fn[i].action),
                         btd_filesystem_get_mountpoint (bfs));
                continue;
            }

            /* run the action and record that we ran it, if it didn't fail to be launched */
            if (action_fn[i].func (self, bfs, record))
                btd_fs_record_set_last_action_time_now (record, action_fn[i].action);
        }
    }

    /* save record & finish */
    if (!btd_fs_record_save (record, &error)) {
        btd_warning ("Unable to save state record for mount '%s': %s",
                     btd_filesystem_get_mountpoint (bfs),
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

    /* we need to be root for the next steps */
    if (!btd_user_is_root ()) {
        g_set_error_literal (error,
                             BTD_BTRFS_ERROR,
                             BTD_BTRFS_ERROR_FAILED,
                             "Need to be root to run this daemon.");
        return FALSE;
    }

    /* check if there is anything for us to do */
    if (priv->mountpoints->len == 0) {
        g_debug ("No mounted Btrfs filesystems found.");
        return TRUE;
    }

    /* run tasks */
    for (guint i = 0; i < priv->mountpoints->len; i++) {
        BtdFilesystem *bfs = g_ptr_array_index (priv->mountpoints, i);
        btd_scheduler_run_for_mount (self, bfs);
    }

    return TRUE;
}

/**
 * btd_scheduler_print_status:
 * @self: An instance of #BtdScheduler
 *
 * Print scheduler status data to stdout.
 *
 * Returns: %TRUE if all data was gathered and no issues were found.
 */
gboolean
btd_scheduler_print_status (BtdScheduler *self)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autoptr(GError) error = NULL;
    gboolean errors_found = FALSE;

    if (priv->mountpoints->len == 0) {
        g_print ("No mounted Btrfs filesystems found.\n");
        return TRUE;
    }

    g_print ("Running on battery: %s\n", btd_machine_is_on_battery () ? "yes" : "no");

    g_print ("Status:\n");
    for (guint i = 0; i < priv->mountpoints->len; i++) {
        BtdFilesystem *bfs = g_ptr_array_index (priv->mountpoints, i);

        g_print ("%c[%dm%s  →  %s%c[%dm\n",
                 0x1B,
                 1,
                 btd_filesystem_get_mountpoint (bfs),
                 btd_filesystem_get_device_name (bfs),
                 0x1B,
                 0);

        for (guint j = BTD_BTRFS_ACTION_UNKNOWN + 1; j < BTD_BTRFS_ACTION_LAST; j++) {
            g_autoptr(BtdFsRecord) record = NULL;
            g_autofree gchar *last_action_time_str = NULL;
            gint64 last_action_timestamp;
            g_autofree gchar *interval_time = btd_humanize_time (
                (gint64) btd_scheduler_get_config_duration_for_action (self, bfs, j));
            g_print ("  • %s\n"
                     "    Runs every %s\n",
                     btd_btrfs_action_to_human_string (j),
                     interval_time);

            record = btd_fs_record_new (btd_filesystem_get_mountpoint (bfs));
            if (!btd_fs_record_load (record, &error)) {
                btd_warning ("Unable to load record for mount '%s': %s",
                             btd_filesystem_get_mountpoint (bfs),
                             error->message);
                g_clear_error (&error);
                errors_found = TRUE;
                continue;
            }

            last_action_timestamp = btd_fs_record_get_last_action_time (record, j);
            if (last_action_timestamp == 0 || btd_fs_record_is_new (record)) {
                last_action_time_str = g_strdup ("Never");
            } else {
                g_autoptr(GDateTime) last_action_dt = NULL;
                last_action_dt = g_date_time_new_from_unix_local (last_action_timestamp);
                last_action_time_str = g_date_time_format (last_action_dt, "%Y-%m-%d %H:%M:%S");
            }
            g_print ("    Last run: %s\n", last_action_time_str);
        }

        g_print ("\n");
    }

    return !errors_found;
}
