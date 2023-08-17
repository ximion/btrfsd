/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define BTD_TYPE_MOUNT_RECORD (btd_mount_record_get_type ())
G_DECLARE_DERIVABLE_TYPE (BtdMountRecord, btd_mount_record, BTD, MOUNT_RECORD, GObject)

struct _BtdMountRecordClass {
    GObjectClass parent_class;
    /*< private >*/
    void (*_as_reserved1) (void);
    void (*_as_reserved2) (void);
    void (*_as_reserved3) (void);
    void (*_as_reserved4) (void);
    void (*_as_reserved5) (void);
    void (*_as_reserved6) (void);
};

BtdMountRecord *btd_mount_record_new (const gchar *mountpoint);

gboolean        btd_mount_record_load (BtdMountRecord *self, GError **error);

const gchar    *btd_mount_record_get_mountpoint (BtdMountRecord *self);
void            btd_mount_record_set_mountpoint (BtdMountRecord *self, const gchar *mount_path);

G_END_DECLS
