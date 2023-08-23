/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:btd-fs-record
 * @short_description: Helper class to store state about a mounted Btrfs filesystem.
 *
 * Store state about a Btrfs mountpoint for later use.
 */

#include "config.h"
#include "btd-fs-record.h"

#include "btd-utils.h"

typedef struct {
    gchar *mountpoint;
    GKeyFile *state;

    gboolean is_new;
} BtdFsRecordPrivate;

enum {
    PROP_0,
    PROP_MOUNTPOINT,
    N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

G_DEFINE_TYPE_WITH_PRIVATE (BtdFsRecord, btd_fs_record, G_TYPE_OBJECT)
#define GET_PRIVATE(o) (btd_fs_record_get_instance_private (o))

/**
 * btd_btrfs_action_to_string:
 * @kind: the %BtdBtrfsAction.
 *
 * Converts the enumerated value to a text representation.
 *
 * Returns: string version of @kind
 **/
const gchar *
btd_btrfs_action_to_string (BtdBtrfsAction kind)
{
    if (kind == BTD_BTRFS_ACTION_STATS)
        return "stats";
    if (kind == BTD_BTRFS_ACTION_SCRUB)
        return "scrub";
    if (kind == BTD_BTRFS_ACTION_BALANCE)
        return "balance";
    return "unknown";
}

/**
 * btd_btrfs_action_from_string:
 * @str: the string.
 *
 * Converts the text representation to an enumerated value.
 *
 * Returns: a #BtdBtrfsAction or %BTD_BTRFS_ACTION_UNKNOWN for unknown.
 **/
BtdBtrfsAction
btd_btrfs_action_from_string (const gchar *str)
{
    if (btd_str_equal0 (str, "stats"))
        return BTD_BTRFS_ACTION_STATS;
    if (btd_str_equal0 (str, "scrub"))
        return BTD_BTRFS_ACTION_SCRUB;
    if (btd_str_equal0 (str, "balance"))
        return BTD_BTRFS_ACTION_BALANCE;
    return BTD_BTRFS_ACTION_UNKNOWN;
}

/**
 * btd_btrfs_action_to_human_string:
 * @kind: the %BtdBtrfsAction.
 *
 * Converts the enumerated value to a human-readable text representation.
 *
 * Returns: string version of @kind
 **/
const gchar *
btd_btrfs_action_to_human_string (BtdBtrfsAction kind)
{
    if (kind == BTD_BTRFS_ACTION_STATS)
        return "Check for Issues";
    if (kind == BTD_BTRFS_ACTION_SCRUB)
        return "Scrub Filesystem";
    if (kind == BTD_BTRFS_ACTION_BALANCE)
        return "Balance Filesystem";
    return "Unknown Action";
}

static void
btd_fs_record_init (BtdFsRecord *self)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);

    priv->state = g_key_file_new ();
}

static void
btd_fs_record_finalize (GObject *object)
{
    BtdFsRecord *bmount = BTD_FS_RECORD (object);
    BtdFsRecordPrivate *priv = GET_PRIVATE (bmount);

    g_free (priv->mountpoint);
    g_key_file_unref (priv->state);

    G_OBJECT_CLASS (btd_fs_record_parent_class)->finalize (object);
}

static void
btd_fs_record_set_property (GObject *object,
                            guint property_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
    BtdFsRecord *self = BTD_FS_RECORD (object);
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);

    switch (property_id) {
    case PROP_MOUNTPOINT:
        g_free (priv->mountpoint);
        priv->mountpoint = g_value_dup_string (value);
        if (btd_is_empty (priv->mountpoint))
            g_critical ("Mountpoint for record file is empty!");
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
btd_fs_record_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    BtdFsRecord *self = BTD_FS_RECORD (object);
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);

    switch (property_id) {
    case PROP_MOUNTPOINT:
        g_value_set_string (value, priv->mountpoint);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
btd_fs_record_class_init (BtdFsRecordClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    object_class->finalize = btd_fs_record_finalize;
    object_class->set_property = btd_fs_record_set_property;
    object_class->get_property = btd_fs_record_get_property;

    obj_properties[PROP_MOUNTPOINT] = g_param_spec_string ("mountpoint",
                                                           "Mountpoint",
                                                           "Mountpoint property",
                                                           NULL,
                                                           G_PARAM_READWRITE |
                                                               G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPERTIES, obj_properties);
}

/**
 * btd_fs_record_new:
 *
 * Creates a new #BtdFsRecord.
 *
 * Returns: (transfer full): a #BtdFsRecord
 */
BtdFsRecord *
btd_fs_record_new (const gchar *mountpoint)
{
    BtdFsRecord *self;
    self = g_object_new (BTD_TYPE_FS_RECORD, "mountpoint", mountpoint, NULL);
    return BTD_FS_RECORD (self);
}

static gchar *
btd_fs_record_get_state_filename (BtdFsRecord *self)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *state_path = NULL;
    g_autofree gchar *state_filename = NULL;
    g_autofree gchar *btrfsd_state_dir = NULL;

    state_filename = btd_path_to_filename (priv->mountpoint);
    btrfsd_state_dir = btd_get_state_dir ();
    state_path = g_strconcat (btrfsd_state_dir, "/", state_filename, ".state", NULL);

    return g_steal_pointer (&state_path);
}

/**
 * btd_fs_record_load:
 * @self: An instance of #BtdFsRecord.
 * @error: A #GError
 *
 * Load mount record.
 *
 * Returns: %TRUE on success.
 */
gboolean
btd_fs_record_load (BtdFsRecord *self, GError **error)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *state_path = NULL;

    state_path = btd_fs_record_get_state_filename (self);
    if (g_file_test (state_path, G_FILE_TEST_EXISTS)) {
        if (!g_key_file_load_from_file (priv->state, state_path, G_KEY_FILE_NONE, error))
            return FALSE;
    } else {
        priv->is_new = TRUE;

        /* the file did not exist yet, so we cheat and assume all jobs recently ran, so we don't run every job immediately */
        for (guint j = BTD_BTRFS_ACTION_UNKNOWN + 1; j < BTD_BTRFS_ACTION_LAST; j++) {
            /* running "stats" immediately is fine, the more expensive actions should wait */
            if (j != BTD_BTRFS_ACTION_STATS)
                btd_fs_record_set_last_action_time_now (self, j);
        }
    }

    return TRUE;
}

/**
 * btd_fs_record_save:
 * @self: An instance of #BtdFsRecord.
 * @error: A #GError
 *
 * Save mount record.
 *
 * Returns: %TRUE on success.
 */
gboolean
btd_fs_record_save (BtdFsRecord *self, GError **error)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    g_autofree gchar *state_path = NULL;

    state_path = btd_fs_record_get_state_filename (self);
    return g_key_file_save_to_file (priv->state, state_path, error);
}

/**
 * btd_fs_record_is_new:
 * @self: An instance of #BtdFsRecord.
 *
 * Check if the record file was just created.
 *
 * returns: %TRUE if the state record is new.
 */
gboolean
btd_fs_record_is_new (BtdFsRecord *self)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    return priv->is_new;
}

/**
 * btd_fs_record_get_mountpoint:
 * @self: An instance of #BtdFsRecord.
 *
 * Returns: The mountpoint path.
 */
const gchar *
btd_fs_record_get_mountpoint (BtdFsRecord *self)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    return priv->mountpoint;
}

/**
 * btd_fs_record_set_mountpoint:
 * @self: An instance of #BtdFsRecord.
 * @mount_path: The new mountpoint path.
 *
 * Set a new mountpoint path.
 */
void
btd_fs_record_set_mountpoint (BtdFsRecord *self, const gchar *mount_path)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    g_free (priv->mountpoint);
    priv->mountpoint = g_strdup (mount_path);
}

/**
 * btd_fs_record_get_last_action_time:
 * @self: An instance of #BtdFsRecord.
 * @action_kind: The %BtdBtrfsAction to check for.
 *
 * Returns: Last UNIX timestamp when the action was run, or 0 if never.
 */
gint64
btd_fs_record_get_last_action_time (BtdFsRecord *self, BtdBtrfsAction action_kind)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    return g_key_file_get_int64 (priv->state,
                                 "times",
                                 btd_btrfs_action_to_string (action_kind),
                                 NULL);
}

/**
 * btd_fs_record_set_last_action_time_now:
 * @self: An instance of #BtdFsRecord.
 * @action_kind: The %BtdBtrfsAction.
 *
 * Set the last time the action was performed to now.
 */
void
btd_fs_record_set_last_action_time_now (BtdFsRecord *self, BtdBtrfsAction action_kind)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    g_key_file_set_uint64 (priv->state,
                           "times",
                           btd_btrfs_action_to_string (action_kind),
                           (gint64) time (NULL));
}

/**
 * btd_fs_record_get_value_int:
 * @self: An instance of #BtdFsRecord.
 * @group_name: The group.
 * @key: The key to look at.
 * @default_value: A default value.
 *
 * Returns: The selected value.
 */
gint64
btd_fs_record_get_value_int (BtdFsRecord *self,
                             const gchar *group_name,
                             const gchar *key,
                             gint64 default_value)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    gint64 value;
    g_autoptr(GError) error = NULL;

    value = g_key_file_get_int64 (priv->state, group_name, key, &error);
    if (error != NULL)
        return default_value;
    return value;
}

/**
 * btd_fs_record_get_value_int:
 * @self: An instance of #BtdFsRecord.
 * @group_name: The group.
 * @key: The key to look at.
 * @default_value: A default value.
 *
 * Set en integer value in the state record.
 */
void
btd_fs_record_set_value_int (BtdFsRecord *self,
                             const gchar *group_name,
                             const gchar *key,
                             gint64 value)
{
    BtdFsRecordPrivate *priv = GET_PRIVATE (self);
    g_key_file_set_uint64 (priv->state, group_name, key, value);
}
