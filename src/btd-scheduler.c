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
    time_t reference_time;

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

    /* The origin reference time when the scheduler was loaded, "current time".
     * We reduce the current time by a minute, just in case there were any delays after cron
     * called us, so we for sure run all remaining tasks if we are called again in an hour. */
    priv->reference_time = time (NULL) - 60;

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
        "never");

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
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autoptr(GBytes) template_bytes = NULL;
    g_autofree gchar *mail_from = NULL;
    g_autofree gchar *formatted_time = NULL;
    g_autofree gchar *mail_body = NULL;
    g_autofree gchar *fs_usage = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GDateTime) dt_now = g_date_time_new_now_local ();
    time_t time_last_mail;

    time_last_mail = btd_fs_record_get_value_int (record, "messages", "issue_mail_sent", 0);
    if (!new_errors_found && (priv->reference_time - time_last_mail) < SECONDS_IN_AN_HOUR * 20) {
        g_autofree gchar *time_waiting_str = btd_humanize_time (
            (SECONDS_IN_AN_HOUR * 20) - (priv->reference_time - time_last_mail));
        btd_debug ("Issue email for '%s' already sent and no new issues found, will send "
                   "a reminder in %s if the issues persist.",
                   btd_filesystem_get_mountpoint (bfs),
                   time_waiting_str);
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
    btd_fs_record_set_value_int (record, "messages", "issue_mail_sent", priv->reference_time);

    return TRUE;
}

static gboolean
btd_scheduler_run_stats (BtdScheduler *self, BtdFilesystem *bfs, BtdFsRecord *record)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *mail_address = NULL;
    g_autofree gchar *issue_report = NULL;
    guint64 error_count = 0;
    guint64 prev_error_count = 0;
    g_autoptr(GError) error = NULL;

    btd_debug ("Reading stats for %s", btd_filesystem_get_mountpoint (bfs));

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

    btd_debug ("Found %" G_GUINT64_FORMAT " errors for %s",
               error_count,
               btd_filesystem_get_mountpoint (bfs));

    if (error_count > prev_error_count ||
        (priv->reference_time -
             btd_fs_record_get_value_int (record, "messages", "broadcast_sent", 0) >
         SECONDS_IN_AN_HOUR * 6)) {
        g_autofree gchar *bc_message = NULL;
        /* broadcast message that there are errors to be fixed, do that roughly every 6h */
        bc_message = g_strdup_printf ("⚠ Errors detected on filesystem at %s!\n"
                                      "Please back up your files immediately. You can run "
                                      "`btrfs device stats %s` for details.\n",
                                      btd_filesystem_get_mountpoint (bfs),
                                      btd_filesystem_get_mountpoint (bfs));

        btd_broadcast_message (bc_message);
        btd_fs_record_set_value_int (record, "messages", "broadcast_sent", priv->reference_time);
    }

    if (btd_is_empty (mail_address)) {
        btd_warning ("Errors detected on filesystem '%s'", btd_filesystem_get_mountpoint (bfs));
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

    btd_debug ("Running scrub on filesystem %s", btd_filesystem_get_mountpoint (bfs));
    if (!btd_filesystem_scrub (bfs, &error)) {
        btd_warning ("Scrub on %s failed: %s", btd_filesystem_get_mountpoint (bfs), error->message);
        return FALSE;
    }

    return TRUE;
}

static gboolean
btd_scheduler_run_balance (BtdScheduler *self, BtdFilesystem *bfs, BtdFsRecord *record)
{
    g_autoptr(GError) error = NULL;

    btd_debug ("Running balance on filesystem %s", btd_filesystem_get_mountpoint (bfs));
    if (!btd_filesystem_balance (bfs, &error)) {
        btd_warning ("Balance on %s failed: %s",
                     btd_filesystem_get_mountpoint (bfs),
                     error->message);
        return FALSE;
    }

    return TRUE;
}

static gboolean
btd_scheduler_run_for_mount (BtdScheduler *self, BtdFilesystem *bfs)
{
    BtdSchedulerPrivate *priv = GET_PRIVATE (self);
    g_autoptr(BtdFsRecord) record = NULL;
    g_autoptr(GError) error = NULL;
    gint64 last_time;
    time_t interval_time;
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

    /* run all actions */
    for (guint i = 0; action_fn[i].func != NULL; i++) {
        interval_time = (time_t) btd_scheduler_get_config_duration_for_action (self,
                                                                               bfs,
                                                                               action_fn[i].action);
        if (interval_time == 0) {
            btd_debug ("Skipping %s on %s, action is disabled.",
                       btd_btrfs_action_to_string (action_fn[i].action),
                       btd_filesystem_get_mountpoint (bfs));
            continue;
        }

        last_time = btd_fs_record_get_last_action_time (record, action_fn[i].action);
        if (priv->reference_time - last_time > interval_time) {
            /* first check if this action is even allowed to be run if we are on batter power */
            if (!action_fn[i].allow_on_battery && btd_machine_is_on_battery ()) {
                btd_debug ("Skipping %s on %s, we are running on battery power.",
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

static gint
btd_filesystem_compare (gconstpointer fs_a, gconstpointer fs_b)
{
    const gchar *mount_a = btd_filesystem_get_mountpoint (
        BTD_FILESYSTEM (*(BtdFilesystem **) fs_a));
    const gchar *mount_b = btd_filesystem_get_mountpoint (
        BTD_FILESYSTEM (*(BtdFilesystem **) fs_b));

    return g_strcmp0 (mount_a, mount_b);
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
    g_autoptr(GHashTable) known_devices = g_hash_table_new (g_direct_hash, g_direct_equal);

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

    /* sort mountpoints to get a predictable order */
    g_ptr_array_sort (priv->mountpoints, btd_filesystem_compare);

    /* run tasks */
    for (guint i = 0; i < priv->mountpoints->len; i++) {
        BtdFilesystem *bfs = g_ptr_array_index (priv->mountpoints, i);
        dev_t devno = btd_filesystem_get_devno (bfs);

        if (g_hash_table_contains (known_devices, GUINT_TO_POINTER (devno))) {
            btd_debug ("Skipping %s, filesystem was already handled via a previous mount.",
                       btd_filesystem_get_mountpoint (bfs));
            continue;
        }

        g_hash_table_add (known_devices, GUINT_TO_POINTER (devno));
        btd_scheduler_run_for_mount (self, bfs);
    }

    return TRUE;
}

/**
 * btd_scheduler_print_fs_status_entry:
 * @self: An instance of #BtdScheduler
 * @mountpoints: A #GPtrArray of #BtdFilesystem mountpoints which belong to the same filesystem
 *
 * Print filesystem scheduler status to stadout.
 * Helper function for btd_scheduler_print_status
 *
 * Returns: %TRUE if no issues were found.
 */
static gboolean
btd_scheduler_print_fs_status_entry (BtdScheduler *self, GPtrArray *mountpoints)
{
    BtdFilesystem *bfs;
    gboolean errors_found = FALSE;
    g_autoptr(GError) error = NULL;

    if (mountpoints->len == 0)
        return TRUE;

    bfs = g_ptr_array_index (mountpoints, 0);
    if (mountpoints->len > 1) {
        g_autoptr(GString) mp_list = g_string_new (NULL);
        for (guint i = 1; i < mountpoints->len; i++) {
            BtdFilesystem *secondary = BTD_FILESYSTEM (g_ptr_array_index (mountpoints, i));
            g_string_append (mp_list, btd_filesystem_get_mountpoint (secondary));

            /* append comma if not the last element */
            if (i < mountpoints->len - 1)
                g_string_append_c (mp_list, ',');
        }

        g_print ("%c[%dm%s (%s) →  %s%c[%dm\n",
                 0x1B,
                 1,
                 btd_filesystem_get_mountpoint (bfs),
                 mp_list->str,
                 btd_filesystem_get_device_name (bfs),
                 0x1B,
                 0);
    } else {
        g_print ("%c[%dm%s  →  %s%c[%dm\n",
                 0x1B,
                 1,
                 btd_filesystem_get_mountpoint (bfs),
                 btd_filesystem_get_device_name (bfs),
                 0x1B,
                 0);
    }

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

        if (j == BTD_BTRFS_ACTION_STATS) {
            g_autofree gchar *mail_address = btd_scheduler_get_config_value (self,
                                                                             bfs,
                                                                             "mail_address",
                                                                             NULL);
            if (mail_address != NULL)
                g_print ("    Error mails to: %s\n", mail_address);
        }
    }

    g_print ("\n");

    return !errors_found;
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
    gboolean errors_found = FALSE;
    g_autoptr(GHashTable) devno_map = NULL;
    GHashTableIter ht_iter;
    gpointer ht_value;

    if (priv->mountpoints->len == 0) {
        g_print ("No mounted Btrfs filesystems found.\n");
        return TRUE;
    }

    /* create a map of devno -> mountpoints */
    devno_map = g_hash_table_new_full (g_direct_hash,
                                       g_direct_equal,
                                       NULL,
                                       (GDestroyNotify) g_ptr_array_unref);
    for (guint i = 0; i < priv->mountpoints->len; i++) {
        BtdFilesystem *bfs = BTD_FILESYSTEM (g_ptr_array_index (priv->mountpoints, i));
        GPtrArray *array = NULL;
        dev_t devno = btd_filesystem_get_devno (bfs);

        array = g_hash_table_lookup (devno_map, GUINT_TO_POINTER (devno));
        if (!array) {
            array = g_ptr_array_new_with_free_func (g_object_unref);
            g_hash_table_insert (devno_map, GUINT_TO_POINTER (devno), array);
        }
        g_ptr_array_add (array, g_object_ref (bfs));
    }

    g_print ("Running on battery: %s\n", btd_machine_is_on_battery () ? "yes" : "no");

    g_print ("Status:\n");
    g_hash_table_iter_init (&ht_iter, devno_map);
    while (g_hash_table_iter_next (&ht_iter, NULL, &ht_value)) {
        GPtrArray *mps = (GPtrArray *) ht_value;

        /* sort mountpoints to get a predictable order */
        g_ptr_array_sort (mps, btd_filesystem_compare);

        if (!btd_scheduler_print_fs_status_entry (self, mps))
            errors_found = TRUE;
    }

    return !errors_found;
}
