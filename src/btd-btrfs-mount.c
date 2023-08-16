/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:btd-btrfs-mount
 * @short_description: An active Btrfs mountpoint on the system that can be acted on.
 *
 * Defines an active Btrfs mountpoint on the current system, and can perform
 * various actions on it.
 */

#include "config.h"
#include "btd-btrfs-mount.h"

#include <libmount/libmount.h>
#include <json-glib/json-glib.h>

#include "btd-utils.h"

typedef struct {
    gchar *device_name;
    gchar *mountpoint;
} BtdBtrfsMountPrivate;

enum {
    PROP_0,
    PROP_DEVICE_NAME,
    PROP_MOUNTPOINT,
    N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

G_DEFINE_TYPE_WITH_PRIVATE (BtdBtrfsMount, btd_btrfs_mount, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (btd_btrfs_mount_get_instance_private (o))

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
 * Returns: (transfer container) (element-type BtdBtrfsMount): mounted Btrfs filesystems
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
            BtdBtrfsMount *bmount = btd_btrfs_mount_new (mnt_fs_get_source (fs),
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
btd_btrfs_mount_init (BtdBtrfsMount *self)
{
    BtdBtrfsMountPrivate *priv = GET_PRIVATE (self);

    priv->device_name = NULL;
}

static void
btd_btrfs_mount_finalize (GObject *object)
{
    BtdBtrfsMount *bmount = BTD_BTRFS_MOUNT (object);
    BtdBtrfsMountPrivate *priv = GET_PRIVATE (bmount);

    g_free (priv->device_name);
    g_free (priv->mountpoint);

    G_OBJECT_CLASS (btd_btrfs_mount_parent_class)->finalize (object);
}

static void
btd_btrfs_mount_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
    BtdBtrfsMount *self = BTD_BTRFS_MOUNT (object);
    BtdBtrfsMountPrivate *priv = GET_PRIVATE (self);

    switch (property_id) {
    case PROP_DEVICE_NAME:
        g_free (priv->device_name);
        priv->device_name = g_value_dup_string (value);
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
btd_btrfs_mount_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BtdBtrfsMount *self = BTD_BTRFS_MOUNT (object);
    BtdBtrfsMountPrivate *priv = GET_PRIVATE (self);

    switch (property_id) {
    case PROP_DEVICE_NAME:
        g_value_set_string (value, priv->device_name);
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
btd_btrfs_mount_class_init (BtdBtrfsMountClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = btd_btrfs_mount_finalize;
    object_class->set_property = btd_btrfs_mount_set_property;
    object_class->get_property = btd_btrfs_mount_get_property;

    obj_properties[PROP_DEVICE_NAME] = g_param_spec_string ("device_name",
                                                            "DeviceName",
                                                            "Device property",
                                                            NULL,
                                                            G_PARAM_READWRITE |
                                                                G_PARAM_CONSTRUCT_ONLY);

    obj_properties[PROP_MOUNTPOINT] = g_param_spec_string ("mountpoint",
                                                           "Mountpoint",
                                                           "Mountpoint property",
                                                           NULL,
                                                           G_PARAM_READWRITE |
                                                               G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);
}

/**
 * btd_btrfs_mount_new:
 *
 * Creates a new #BtdBtrfsMount.
 *
 * Returns: (transfer full): a #BtdBtrfsMount
 */
BtdBtrfsMount *
btd_btrfs_mount_new (const gchar *device, const gchar *mountpoint)
{
    BtdBtrfsMount *self;
    self = g_object_new (BTD_TYPE_BTRFS_MOUNT,
                         "device_name",
                         device,
                         "mountpoint",
                         mountpoint,
                         NULL);
    return BTD_BTRFS_MOUNT (self);
}

/**
 * btd_btrfs_mount_get_device_name:
 * @self: An instance of #BtdBtrfsMount.
 *
 * Returns: The device name backing this mountpoint.
 */
const gchar *
btd_btrfs_mount_get_device_name (BtdBtrfsMount *self)
{
    BtdBtrfsMountPrivate *priv = GET_PRIVATE (self);
    return priv->device_name;
}

/**
 * btd_btrfs_mount_get_mountpoint:
 * @self: An instance of #BtdBtrfsMount.
 *
 * Returns: The mountpoint path.
 */
const gchar *
btd_btrfs_mount_get_mountpoint (BtdBtrfsMount *self)
{
    BtdBtrfsMountPrivate *priv = GET_PRIVATE (self);
    return priv->mountpoint;
}

static gchar *
btd_parse_btrfs_device_stats (JsonArray *array, gboolean *errors_found)
{
    g_autoptr(GString) intro_text = NULL;
    g_autoptr(GString) issues_text = NULL;
    gboolean have_errors = FALSE;

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

        /* add device to the known devices list */
        g_string_append_printf (intro_text, "  • %s\n", device);

        /* if there are no errors, we don't add that information to the report */
        if (write_io_errs == 0 && read_io_errs == 0 && flush_io_errs == 0 && corruption_errs == 0 &&
            generation_errs == 0)
            continue;

        /* we have issues, make a full report */
        have_errors = TRUE;
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

    if (errors_found != NULL)
        *errors_found = have_errors;

    /* finalize report */
    if (!have_errors)
        g_string_append (issues_text, "  • No errors found\n");
    g_string_append (intro_text, "\n");
    g_string_prepend (issues_text, intro_text->str);

    return g_string_free (g_steal_pointer (&issues_text), FALSE);
}

/**
 * btd_btrfs_mount_read_error_stats:
 * @self: An instance of #BtdBtrfsMount.
 * @report: (out) (optional) (not nullable): Destination of a string report text.
 * @errors_found: (out) (optional): Set to %TRUE if any errors were detected.
 * @error: A #GError, set if we failed to read statistics.
 *
 * Returns: %TRUE if stats were read successfully.
 */
gboolean
btd_btrfs_mount_read_error_stats (BtdBtrfsMount *self,
                                  gchar **report,
                                  gboolean *errors_found,
                                  GError **error)
{
    BtdBtrfsMountPrivate *priv = GET_PRIVATE (self);
    GError *tmp_error = NULL;
    gint btrfs_exit_code;
    g_autofree gchar *stats_output = NULL;
    g_autofree gchar *stderr_output = NULL;
    g_autoptr(JsonParser) parser = NULL;
    g_autofree gchar *tmp_report = NULL;
    JsonObject *root = NULL;
    JsonArray *device_stats = NULL;

    gchar *command[] = { BTRFS_CMD, "--format=json", "device", "stats", priv->mountpoint, NULL };
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
    tmp_report = btd_parse_btrfs_device_stats (device_stats, errors_found);
    if (report != NULL)
        *report = g_steal_pointer (&tmp_report);

    return TRUE;
}

/**
 * btd_btrfs_mount_scrub:
 * @self: An instance of #BtdBtrfsMount.
 * @error: A #GError, set if scrub failed.
 *
 * Returns: %TRUE if scrub operation completed without errors.
 */
gboolean
btd_btrfs_mount_scrub (BtdBtrfsMount *self, GError **error)
{
    BtdBtrfsMountPrivate *priv = GET_PRIVATE (self);
    GError *tmp_error = NULL;
    gint btrfs_exit_code;
    g_autofree gchar *btrfs_stdout = NULL;
    g_autofree gchar *btrfs_stderr = NULL;

    gchar *command[] = { BTRFS_CMD, "-q", "scrub", "start", "-B", priv->mountpoint, NULL };
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
