/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:btd-filesystem
 * @short_description: An active Btrfs mountpoint on the system.
 *
 * Defines an active Btrfs mountpoint on the current system, and can perform
 * various actions on it.
 */

#include "config.h"
#include "btd-filesystem.h"

#include <libmount/libmount.h>
#include <json-glib/json-glib.h>

#include "btd-utils.h"
#include "btd-logging.h"

typedef struct {
    gchar *device_name;
    gchar *mountpoint;
    dev_t devno;
} BtdFilesystemPrivate;

enum {
    PROP_0,
    PROP_DEVICE_NAME,
    PROP_DEVNO,
    PROP_MOUNTPOINT,
    N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

G_DEFINE_TYPE_WITH_PRIVATE (BtdFilesystem, btd_filesystem, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (btd_filesystem_get_instance_private (o))

/**
 * btd_btrfs_error_quark:
 *
 * Return value: An error quark.
 */
G_DEFINE_QUARK (btd-btrfs-error-quark, btd_btrfs_error)

/**
 * btd_find_mounted_btrfs_filesystems:
 *
 * Find all mounted Btrfs filesystems on the current system.
 *
 * Returns: (transfer container) (element-type BtdFilesystem): mounted Btrfs filesystems
 */
GPtrArray *
btd_find_mounted_btrfs_filesystems (GError **error)
{
    struct libmnt_table *table;
    struct libmnt_iter *iter;
    struct libmnt_fs *fs;
    g_autoptr(GPtrArray) result = NULL;

    table = mnt_new_table ();
    iter = mnt_new_iter (MNT_ITER_FORWARD);

    /* load the current mount table */
    if (mnt_table_parse_mtab (table, NULL) != 0) {
        g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to parse mount table");
        goto out;
    }

    /* enlist all btrfs filesystems */
    result = g_ptr_array_new_with_free_func (g_object_unref);
    while (mnt_table_next_fs (table, iter, &fs) == 0) {
        if (g_strcmp0 (mnt_fs_get_fstype (fs), "btrfs") == 0) {
            BtdFilesystem *bmount = btd_filesystem_new (mnt_fs_get_source (fs),
                                                        mnt_fs_get_devno (fs),
                                                        mnt_fs_get_target (fs));
            g_ptr_array_add (result, bmount);
        }
    }

out:
    mnt_free_iter (iter);
    mnt_free_table (table);

    return g_steal_pointer (&result);
}

static void
btd_filesystem_init (BtdFilesystem *self)
{
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);

    priv->device_name = NULL;
}

static void
btd_filesystem_finalize (GObject *object)
{
    BtdFilesystem *bmount = BTD_FILESYSTEM (object);
    BtdFilesystemPrivate *priv = GET_PRIVATE (bmount);

    g_free (priv->device_name);
    g_free (priv->mountpoint);

    G_OBJECT_CLASS (btd_filesystem_parent_class)->finalize (object);
}

static void
btd_filesystem_set_property (GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    BtdFilesystem *self = BTD_FILESYSTEM (object);
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);

    switch (property_id) {
    case PROP_DEVICE_NAME:
        g_free (priv->device_name);
        priv->device_name = g_value_dup_string (value);
        break;
    case PROP_DEVNO:
        priv->devno = g_value_get_uint64 (value);
        break;
    case PROP_MOUNTPOINT:
        g_free (priv->mountpoint);
        priv->mountpoint = g_value_dup_string (value);
        if (btd_is_empty (priv->mountpoint))
            g_critical ("Mountpoint for %s is empty!", priv->device_name);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
btd_filesystem_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BtdFilesystem *self = BTD_FILESYSTEM (object);
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);

    switch (property_id) {
    case PROP_DEVICE_NAME:
        g_value_set_string (value, priv->device_name);
        break;
    case PROP_DEVNO:
        g_value_set_uint64 (value, priv->devno);
        break;
    case PROP_MOUNTPOINT:
        g_value_set_string (value, priv->mountpoint);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
btd_filesystem_class_init (BtdFilesystemClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = btd_filesystem_finalize;
    object_class->set_property = btd_filesystem_set_property;
    object_class->get_property = btd_filesystem_get_property;

    obj_properties[PROP_DEVICE_NAME] = g_param_spec_string ("device_name",
                                                            "DeviceName",
                                                            "Device property",
                                                            NULL,
                                                            G_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT_ONLY);

    obj_properties[PROP_DEVNO] = g_param_spec_uint64 ("devno",
                                                      "Devno",
                                                      "Physical device number",
                                                      0,
                                                      G_MAXUINT64,
                                                      0,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    obj_properties[PROP_MOUNTPOINT] = g_param_spec_string ("mountpoint",
                                                           "Mountpoint",
                                                           "Mountpoint property",
                                                           NULL,
                                                           G_PARAM_READWRITE |
                                                               G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);
}

/**
 * btd_filesystem_new:
 *
 * Creates a new #BtdFilesystem.
 *
 * Returns: (transfer full): a #BtdFilesystem
 */
BtdFilesystem *
btd_filesystem_new (const gchar *device, dev_t devno, const gchar *mountpoint)
{
    BtdFilesystem *self;
    self = g_object_new (BTD_TYPE_FILESYSTEM,
                         "device_name",
                         device,
                         "devno",
                         (guint64) devno,
                         "mountpoint",
                         mountpoint,
                         NULL);
    return BTD_FILESYSTEM (self);
}

/**
 * btd_filesystem_get_device_name:
 * @self: An instance of #BtdFilesystem.
 *
 * Returns: The device name backing this mountpoint.
 */
const gchar *
btd_filesystem_get_device_name (BtdFilesystem *self)
{
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);
    return priv->device_name;
}

/**
 * btd_filesystem_get_mountpoint:
 * @self: An instance of #BtdFilesystem.
 *
 * Returns: The mountpoint path.
 */
const gchar *
btd_filesystem_get_mountpoint (BtdFilesystem *self)
{
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);
    return priv->mountpoint;
}

/**
 * btd_filesystem_get_devno:
 * @self: An instance of #BtdFilesystem.
 *
 * Returns: The device number.
 */
dev_t
btd_filesystem_get_devno (BtdFilesystem *self)
{
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);
    return priv->devno;
}

/**
 * btd_filesystem_read_usage:
 * @self: An instance of #BtdFilesystem.
 * @error: A #GError
 *
 * Read filesystem usage information (btrfs fi df).
 *
 * Returns: The Btrfs usage report.
 */
gchar *
btd_filesystem_read_usage (BtdFilesystem *self, GError **error)
{
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);
    GError *tmp_error = NULL;
    gint btrfs_exit_code;
    g_autofree gchar *df_output = NULL;
    g_autofree gchar *stderr_output = NULL;

    gchar *command[] = { BTRFS_CMD, "fi", "df", priv->mountpoint, NULL };
    if (!g_spawn_sync (NULL, /* working directory */
                       command,
                       NULL, /* envp */
                       G_SPAWN_DEFAULT,
                       NULL,
                       NULL,
                       &df_output,
                       &stderr_output,
                       &btrfs_exit_code,
                       &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error, "Failed to execute btrfs fi df command:");
        return FALSE;
    }

    if (btrfs_exit_code != 0) {
        g_set_error (error,
                     BTD_BTRFS_ERROR,
                     BTD_BTRFS_ERROR_FAILED,
                     "Running btrfs fi df has failed: %s",
                     btd_strstripnl (stderr_output));
        return FALSE;
    }

    return btd_strstripnl (g_steal_pointer (&df_output));
}

static gchar *
btd_parse_btrfs_device_stats (JsonArray *array, guint64 *errors_count)
{
    g_autoptr(GString) intro_text = NULL;
    g_autoptr(GString) issues_text = NULL;
    guint64 total_errors = 0;

    intro_text = g_string_new ("Registered Devices:\n");
    issues_text = g_string_new ("Issue Report:\n");

    for (guint i = 0; i < json_array_get_length (array); i++) {
        JsonObject *obj = json_array_get_object_element (array, i);

        const gchar *device = json_object_get_string_member (obj, "device");
        const gchar *devid = json_object_get_string_member (obj, "devid");
        gint64 write_io_errs = json_object_get_int_member (obj, "write_io_errs");
        gint64 read_io_errs = json_object_get_int_member (obj, "read_io_errs");
        gint64 flush_io_errs = json_object_get_int_member (obj, "flush_io_errs");
        gint64 corruption_errs = json_object_get_int_member (obj, "corruption_errs");
        gint64 generation_errs = json_object_get_int_member (obj, "generation_errs");

        total_errors = write_io_errs + read_io_errs + flush_io_errs + corruption_errs +
                       generation_errs;
        if (errors_count != NULL)
            *errors_count = total_errors;

        /* add device to the known devices list */
        g_string_append_printf (intro_text, "  • %s\n", device);

        /* if there are no errors, we don't add that information to the report */
        if (total_errors == 0)
            continue;

        /* we have issues, make a full report */
        g_string_append_printf (issues_text, "Device: %s\n", device);
        g_string_append_printf (issues_text, "Devid:  %s\n", devid);
        g_string_append_printf (issues_text,
                                "Write IO Errors: %" G_GINT64_FORMAT "\n",
                                write_io_errs);
        g_string_append_printf (issues_text,
                                "Read IO Errors:  %" G_GINT64_FORMAT "\n",
                                read_io_errs);
        g_string_append_printf (issues_text,
                                "Flush IO Errors: %" G_GINT64_FORMAT "\n",
                                flush_io_errs);
        g_string_append_printf (issues_text,
                                "Corruption Errors: %" G_GINT64_FORMAT "\n",
                                corruption_errs);
        g_string_append_printf (issues_text,
                                "Generation Errors: %" G_GINT64_FORMAT "\n\n",
                                generation_errs);
    }

    /* finalize report */
    if (total_errors == 0)
        g_string_append (issues_text, "  • No errors found\n");
    g_string_append (intro_text, "\n");
    g_string_prepend (issues_text, intro_text->str);

    /* drop trailing newlines */
    g_string_truncate (issues_text, issues_text->len - 2);

    return g_string_free (g_steal_pointer (&issues_text), FALSE);
}

/**
 * btd_filesystem_read_error_stats:
 * @self: An instance of #BtdFilesystem.
 * @report: (out) (optional) (not nullable): Destination of a string report text.
 * @errors_count: (out) (optional): Number of detected erros
 * @error: A #GError, set if we failed to read statistics.
 *
 * Returns: %TRUE if stats were read successfully.
 */
gboolean
btd_filesystem_read_error_stats (BtdFilesystem *self,
                                 gchar **report,
                                 guint64 *errors_count,
                                 GError **error)
{
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);
    GError *tmp_error = NULL;
    gint btrfs_exit_code;
    g_autofree gchar *stats_output = NULL;
    g_autofree gchar *stderr_output = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autofree gchar *tmp_report = NULL;
    JsonObject *root = NULL;
    JsonArray *device_stats = NULL;

    gchar *command[] = { BTRFS_CMD, "--format=json", "device", "stats", priv->mountpoint, NULL };
    btd_debug ("Running btrfs device stats on %s", priv->mountpoint);
    if (!g_spawn_sync (NULL, /* working directory */
                       command,
                       NULL, /* envp */
                       G_SPAWN_DEFAULT,
                       NULL,
                       NULL,
                       &stats_output,
                       &stderr_output,
                       &btrfs_exit_code,
                       &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error, "Failed to execute btrfs stats command:");
        return FALSE;
    }

    if (btrfs_exit_code != 0) {
        g_set_error (error,
                     BTD_BTRFS_ERROR,
                     BTD_BTRFS_ERROR_FAILED,
                     "Running btrfs stats has failed: %s",
                     btd_strstripnl (stderr_output));
        return FALSE;
    }

    parser = json_parser_new ();
    if (!json_parser_load_from_data (parser, stats_output, -1, &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error, "Failed to parse btrfs stats JSON:");
        return FALSE;
    }

    root = json_node_get_object (json_parser_get_root (parser));
    device_stats = json_object_get_array_member (root, "device-stats");
    if (device_stats == NULL) {
        g_set_error_literal (error,
                             BTD_BTRFS_ERROR,
                             BTD_BTRFS_ERROR_PARSE,
                             "Failed to parse stats output: No 'device-stats' section.");
        return FALSE;
    }

    /* parse stats & generate report */
    tmp_report = btd_parse_btrfs_device_stats (device_stats, errors_count);
    if (report != NULL)
        *report = g_steal_pointer (&tmp_report);

    return TRUE;
}

/**
 * btd_filesystem_scrub:
 * @self: An instance of #BtdFilesystem.
 * @error: A #GError, set if scrub failed.
 *
 * Returns: %TRUE if scrub operation completed without errors.
 */
gboolean
btd_filesystem_scrub (BtdFilesystem *self, GError **error)
{
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);
    GError *tmp_error = NULL;
    gint btrfs_exit_code;
    g_autofree gchar *btrfs_stdout = NULL;
    g_autofree gchar *btrfs_stderr = NULL;

    gchar *command[] = { BTRFS_CMD, "-q", "scrub", "start", "-B", priv->mountpoint, NULL };
    btd_info ("Running btrfs scrub on %s", priv->mountpoint);
    if (!g_spawn_sync (NULL, /* working directory */
                       command,
                       NULL, /* envp */
                       G_SPAWN_DEFAULT,
                       NULL,
                       NULL,
                       &btrfs_stdout,
                       &btrfs_stderr,
                       &btrfs_exit_code,
                       &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error, "Failed to execute btrfs scrub command:");
        return FALSE;
    }

    if (btrfs_exit_code != 0) {
        g_autofree gchar *output_msg = NULL;
        btrfs_stdout = btd_strstripnl (btrfs_stdout);
        btrfs_stderr = btd_strstripnl (btrfs_stderr);

        if (btd_is_empty (btrfs_stdout))
            output_msg = g_steal_pointer (&btrfs_stderr);
        else if (btd_is_empty (btrfs_stderr))
            output_msg = g_steal_pointer (&btrfs_stdout);
        else
            output_msg = g_strconcat (btrfs_stderr, "\n", btrfs_stdout, NULL);

        g_set_error (error,
                     BTD_BTRFS_ERROR,
                     BTD_BTRFS_ERROR_SCRUB_FAILED,
                     "Scrub action failed: %s",
                     output_msg);
        return FALSE;
    }

    /* scrub ran successfully */
    return TRUE;
}

/**
 * btd_filesystem_balance:
 * @self: An instance of #BtdFilesystem.
 * @error: A #GError, set if scrub failed.
 *
 * Run balance operation with some sensible defaults.
 *
 * Returns: %TRUE if balance operation completed without errors.
 */
gboolean
btd_filesystem_balance (BtdFilesystem *self, GError **error)
{
    BtdFilesystemPrivate *priv = GET_PRIVATE (self);
    GError *tmp_error = NULL;
    gint btrfs_exit_code;
    g_autofree gchar *btrfs_stdout = NULL;
    g_autofree gchar *btrfs_stderr = NULL;

    gchar *command[] = { BTRFS_CMD,    "balance",    "start",          "--enqueue",
                         "-dusage=15", "-musage=10", priv->mountpoint, NULL };
    btd_info ("Running btrfs balance on %s", priv->mountpoint);
    if (!g_spawn_sync (NULL, /* working directory */
                       command,
                       NULL, /* envp */
                       G_SPAWN_DEFAULT,
                       NULL,
                       NULL,
                       &btrfs_stdout,
                       &btrfs_stderr,
                       &btrfs_exit_code,
                       &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error, "Failed to execute btrfs balance command:");
        return FALSE;
    }

    if (btrfs_exit_code != 0) {
        g_autofree gchar *output_msg = NULL;
        btrfs_stdout = btd_strstripnl (btrfs_stdout);
        btrfs_stderr = btd_strstripnl (btrfs_stderr);

        if (btd_is_empty (btrfs_stdout))
            output_msg = g_steal_pointer (&btrfs_stderr);
        else if (btd_is_empty (btrfs_stderr))
            output_msg = g_steal_pointer (&btrfs_stdout);
        else
            output_msg = g_strconcat (btrfs_stderr, "\n", btrfs_stdout, NULL);

        g_set_error (error,
                     BTD_BTRFS_ERROR,
                     BTD_BTRFS_ERROR_SCRUB_FAILED,
                     "Balance action failed: %s",
                     output_msg);
        return FALSE;
    }

    /* balance ran successfully */
    return TRUE;
}
