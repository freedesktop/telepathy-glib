/*
 * dbus-daemon.h - Header for TpDBusDaemon
 *
 * Copyright (C) 2005-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2005-2009 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __TELEPATHY_DBUS_DAEMON_H__
#define __TELEPATHY_DBUS_DAEMON_H__

#ifndef __TP_IN_DBUS_H__
#error dbus-daemon.h not to be used directly, #include <telepathy-glib/dbus.h>
#endif

#include <telepathy-glib/defs.h>
#include <telepathy-glib/proxy.h>

G_BEGIN_DECLS

/* TpDBusDaemon is typedef'd in proxy.h */
typedef struct _TpDBusDaemonPrivate TpDBusDaemonPrivate;
typedef struct _TpDBusDaemonClass TpDBusDaemonClass;
GType tp_dbus_daemon_get_type (void);

#define TP_TYPE_DBUS_DAEMON \
  (tp_dbus_daemon_get_type ())
#define TP_DBUS_DAEMON(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), TP_TYPE_DBUS_DAEMON, \
                              TpDBusDaemon))
#define TP_DBUS_DAEMON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), TP_TYPE_DBUS_DAEMON, \
                           TpDBusDaemonClass))
#define TP_IS_DBUS_DAEMON(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), TP_TYPE_DBUS_DAEMON))
#define TP_IS_DBUS_DAEMON_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), TP_TYPE_DBUS_DAEMON))
#define TP_DBUS_DAEMON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), TP_TYPE_DBUS_DAEMON, \
                              TpDBusDaemonClass))

TpDBusDaemon *tp_dbus_daemon_dup (GError **error) G_GNUC_WARN_UNUSED_RESULT;

TpDBusDaemon *tp_dbus_daemon_new (GDBusConnection *connection)
  G_GNUC_WARN_UNUSED_RESULT;

gboolean tp_dbus_daemon_request_name (GDBusConnection *dbus_connection,
    const gchar *well_known_name, gboolean idempotent, GError **error);
gboolean tp_dbus_daemon_release_name (GDBusConnection *dbus_connection,
    const gchar *well_known_name, GError **error);

const gchar *tp_dbus_daemon_get_unique_name (GDBusConnection *dbus_connection);

void tp_dbus_daemon_register_object (GDBusConnection *dbus_connection,
    const gchar *object_path, gpointer object);
gboolean tp_dbus_daemon_try_register_object (GDBusConnection *dbus_connection,
    const gchar *object_path,
    gpointer object,
    GError **error);
void tp_dbus_daemon_unregister_object (GDBusConnection *dbus_connection,
    gpointer object);

G_END_DECLS

#endif
