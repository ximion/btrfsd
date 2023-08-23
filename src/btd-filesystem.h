/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * BtdBtrfsError:
 * @BTD_BTRFS_ERROR_FAILED:        Generic failure
 * @BTD_BTRFS_ERROR_PARSE:         Data parsing failed
 * @BTD_BTRFS_ERROR_SCRUB_FAILED:  Scrub operation failed.
 *
 * The error type.
 **/
typedef enum {
    BTD_BTRFS_ERROR_FAILED,
    BTD_BTRFS_ERROR_PARSE,
    BTD_BTRFS_ERROR_SCRUB_FAILED,
    /*< private >*/
    BTD_BTRFS_ERROR_LAST
} BtdBtrfsError;

#define BTD_BTRFS_ERROR btd_btrfs_error_quark ()
GQuark btd_btrfs_error_quark (void);

#define BTD_TYPE_FILESYSTEM (btd_filesystem_get_type ())
G_DECLARE_DERIVABLE_TYPE (BtdFilesystem, btd_filesystem, BTD, FILESYSTEM, GObject)

struct _BtdFilesystemClass {
    GObjectClass parent_class;
    /*< private >*/
    void (*_as_reserved1) (void);
    void (*_as_reserved2) (void);
    void (*_as_reserved3) (void);
    void (*_as_reserved4) (void);
    void (*_as_reserved5) (void);
    void (*_as_reserved6) (void);
};

GPtrArray     *btd_find_mounted_btrfs_filesystems (GError **error);

BtdFilesystem *btd_filesystem_new (const gchar *device, const gchar *mountpoint);

const gchar   *btd_filesystem_get_device_name (BtdFilesystem *self);
const gchar   *btd_filesystem_get_mountpoint (BtdFilesystem *self);

gchar         *btd_filesystem_read_usage (BtdFilesystem *self, GError **error);

gboolean       btd_filesystem_read_error_stats (BtdFilesystem *self,
                                                gchar        **report,
                                                guint64       *errors_count,
                                                GError       **error);

gboolean       btd_filesystem_scrub (BtdFilesystem *self, GError **error);

G_END_DECLS
