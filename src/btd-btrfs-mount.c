/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:btd-btrfs-mount
 * @short_description: An active Btrfs mountpoint on the system that can be acted on.
 * @include: appstream.h
 *
 * Defines an active Btrfs mountpoint on the current system, and can perform
 * various actions on it.
 */

#include "config.h"
#include "btd-btrfs-mount.h"

#include <libmount/libmount.h>

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
 * Get the device name backing this mountpoint.
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
 * Get the mountpoint path.
 */
const gchar *
btd_btrfs_mount_get_mountpoint (BtdBtrfsMount *self)
{
    BtdBtrfsMountPrivate *priv = GET_PRIVATE (self);
    return priv->mountpoint;
}
