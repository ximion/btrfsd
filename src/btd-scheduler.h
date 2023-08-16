/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define BTD_TYPE_SCHEDULER (btd_scheduler_get_type ())
G_DECLARE_DERIVABLE_TYPE (BtdScheduler, btd_scheduler, BTD, SCHEDULER, GObject)

struct _BtdSchedulerClass {
    GObjectClass parent_class;
    /*< private >*/
    void (*_as_reserved1) (void);
    void (*_as_reserved2) (void);
    void (*_as_reserved3) (void);
    void (*_as_reserved4) (void);
    void (*_as_reserved5) (void);
    void (*_as_reserved6) (void);
};

BtdScheduler *btd_scheduler_new (void);
gboolean      btd_scheduler_load (BtdScheduler *self, GError **error);

gboolean      btd_scheduler_run (BtdScheduler *self, GError **error);

G_END_DECLS
