/*
 * Copyright (C) Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:btd-mailer
 * @short_description: Send E-Mail messages.
 *
 * Helper functions to send E-Mail messages.
 */

#include "config.h"
#include "btd-mailer.h"

#include <glib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <utmp.h>
#include <fcntl.h>

/**
 * btd_mail_error_quark:
 *
 * Return value: An error quark.
 */
G_DEFINE_QUARK (btd-mail-error-quark, btd_mail_error)

/**
 * btd_have_sendmail:
 *
 * Returns: %TRUE if the sendmail program is available.
 */
gboolean
btd_have_sendmail (void)
{
    g_autofree gchar *sendmail_exe = NULL;
    sendmail_exe = g_find_program_in_path ("sendmail");
    return sendmail_exe != NULL;
}

/**
 * btd_send_email:
 * @to_address: Address to send the mail to.
 * @body: The message body, including the subject line
 * @error: A #GError.
 *
 * Send any E-Mail via sendmail.
 *
 * Return: %TRUE on success.
 */
gboolean
btd_send_email (const gchar *to_address, const gchar *body, GError **error)
{
    g_autofree gchar *sendmail_exe = NULL;
    g_autofree gchar *email_content = NULL;
    g_autoptr(GError) tmp_error = NULL;
    const gchar *sm_argv[3];
    GPid child_pid;
    gint stdin_fd;
    gint exit_status;

    sendmail_exe = g_find_program_in_path ("sendmail");
    if (sendmail_exe == NULL) {
        g_set_error_literal (error,
                             BTD_MAIL_ERROR,
                             BTD_MAIL_ERROR_FAILED,
                             "Unable to find the `sendmail` command, can not send emails.");
        return FALSE;
    }

    email_content = g_strdup_printf ("To: %s\n%s", to_address, body);

    sm_argv[0] = sendmail_exe;
    sm_argv[1] = "-t";
    sm_argv[2] = NULL;

    if (!g_spawn_async_with_pipes (NULL,
                                   (gchar **) sm_argv,
                                   NULL,
                                   G_SPAWN_DO_NOT_REAP_CHILD,
                                   NULL,
                                   NULL,
                                   &child_pid,
                                   &stdin_fd,
                                   NULL,
                                   NULL,
                                   &tmp_error)) {
        g_propagate_prefixed_error (error, tmp_error, "Failed to send mail with sendmail:");
        return FALSE;
    }

    /* write email content to stdin of sendmail */
    (void) write (stdin_fd, email_content, strlen (email_content));
    close (stdin_fd);

    g_spawn_close_pid (child_pid);
    waitpid (child_pid, &exit_status, 0);

    if (exit_status != 0) {
        g_set_error (error,
                     BTD_MAIL_ERROR,
                     BTD_MAIL_ERROR_FAILED,
                     "Sendmail failed with exist status %d",
                     exit_status);
        return FALSE;
    }

    return TRUE;
}

/**
 * btd_broadcast_message:
 * @message: The message to send.
 *
 * Broadcast a message to all users, like `wall` does.
 */
void
btd_broadcast_message (const gchar *message)
{
    struct utmp *utmp_entry;
    gint fd;

    /* open the utmp file */
    setutent ();

    /* read each entry from the utmp file */
    while ((utmp_entry = getutent ()) != NULL) {
        /* check if the entry is a user process */
        if (utmp_entry->ut_type == USER_PROCESS) {
            /* open the terminal device */
            g_autofree gchar *term_path = g_strconcat ("/dev/", utmp_entry->ut_line, NULL);
            fd = open (term_path, O_WRONLY);
            if (fd != -1) {
                /* write the message to the terminal */
                (void) write (fd, message, strlen (message));
                close (fd);
            }
        }
    }

    /* close the utmp file */
    endutent ();
}
