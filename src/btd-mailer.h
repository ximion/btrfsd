/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

/**
 * BtdMailError:
 * @BTD_MAIL_ERROR_FAILED:        Generic failure
 *
 * The error type.
 **/
typedef enum {
    BTD_MAIL_ERROR_FAILED,
    /*< private >*/
    BTD_MAIL_ERROR_LAST
} BtdMailError;

#define BTD_MAIL_ERROR btd_mail_error_quark ()
GQuark   btd_mail_error_quark (void);

gboolean btd_have_sendmail (void);
gboolean btd_send_email (const gchar *to_address, const gchar *body, GError **error);

G_END_DECLS
