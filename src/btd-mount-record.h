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

/**
 * BtdBtrfsAction:
 * @BTD_BTRFS_ACTION_UNKNOWN: Unknown action
 * @BTD_BTRFS_ACTION_STATS:   Stats action
 * @BTD_BTRFS_ACTION_SCRUB:   Scrub action
 * @BTD_BTRFS_ACTION_BALANCE: Balance action
 *
 * A Btrfs action that we perform.
 **/
typedef enum {
    BTD_BTRFS_ACTION_UNKNOWN,
    BTD_BTRFS_ACTION_STATS,
    BTD_BTRFS_ACTION_SCRUB,
    BTD_BTRFS_ACTION_BALANCE,
    /*< private >*/
    BTD_BTRFS_ACTION_LAST
} BtdBtrfsAction;

const gchar    *btd_btrfs_action_to_string (BtdBtrfsAction kind);
BtdBtrfsAction  btd_btrfs_action_from_string (const gchar *str);
const gchar    *btd_btrfs_action_to_human_string (BtdBtrfsAction kind);

BtdMountRecord *btd_mount_record_new (const gchar *mountpoint);

gboolean        btd_mount_record_load (BtdMountRecord *self, GError **error);
gboolean        btd_mount_record_save (BtdMountRecord *self, GError **error);
gboolean        btd_mount_record_is_new (BtdMountRecord *self);

const gchar    *btd_mount_record_get_mountpoint (BtdMountRecord *self);
void            btd_mount_record_set_mountpoint (BtdMountRecord *self, const gchar *mount_path);

gint64 btd_mount_record_get_last_action_time (BtdMountRecord *self, BtdBtrfsAction action_kind);
void   btd_mount_record_set_last_action_time_now (BtdMountRecord *self, BtdBtrfsAction action_kind);

gint64 btd_mount_record_get_value_int (BtdMountRecord *self,
                                       const gchar    *group_name,
                                       const gchar    *key,
                                       gint64          default_value);
void   btd_mount_record_set_value_int (BtdMountRecord *self,
                                       const gchar    *group_name,
                                       const gchar    *key,
                                       gint64          value);

G_END_DECLS
