/*
 * conn.c - an example connection
 *
 * Copyright © 2007-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright © 2007-2009 Nokia Corporation
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

#include "config.h"

#include "conn.h"

#include <dbus/dbus-glib.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/telepathy-glib-dbus.h>

#include "call-manager.h"
#include "protocol.h"

static void init_presence (gpointer g_iface,
    gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (ExampleCallConnection, example_call_connection,
    TP_TYPE_BASE_CONNECTION,
    G_IMPLEMENT_INTERFACE (TP_TYPE_PRESENCE_MIXIN, init_presence))

enum
{
  PROP_ACCOUNT = 1,
  PROP_SIMULATION_DELAY,
  N_PROPS
};

enum
{
  SIGNAL_AVAILABLE,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

struct _ExampleCallConnectionPrivate
{
  gchar *account;
  guint simulation_delay;
  gboolean away;
  gchar *presence_message;
};

static void
example_call_connection_init (ExampleCallConnection *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      EXAMPLE_TYPE_CALL_CONNECTION,
      ExampleCallConnectionPrivate);
  self->priv->away = FALSE;
  self->priv->presence_message = g_strdup ("");
}

static void
get_property (GObject *object,
              guint property_id,
              GValue *value,
              GParamSpec *spec)
{
  ExampleCallConnection *self = EXAMPLE_CALL_CONNECTION (object);

  switch (property_id)
    {
    case PROP_ACCOUNT:
      g_value_set_string (value, self->priv->account);
      break;

    case PROP_SIMULATION_DELAY:
      g_value_set_uint (value, self->priv->simulation_delay);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
    }
}

static void
set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *spec)
{
  ExampleCallConnection *self = EXAMPLE_CALL_CONNECTION (object);

  switch (property_id)
    {
    case PROP_ACCOUNT:
      g_free (self->priv->account);
      self->priv->account = g_value_dup_string (value);
      break;

    case PROP_SIMULATION_DELAY:
      self->priv->simulation_delay = g_value_get_uint (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, spec);
    }
}

static void
constructed (GObject *object)
{
  void (*chain_up) (GObject *) =
    G_OBJECT_CLASS (example_call_connection_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  tp_presence_mixin_init (TP_PRESENCE_MIXIN (object));
}

static void
finalize (GObject *object)
{
  ExampleCallConnection *self = EXAMPLE_CALL_CONNECTION (object);

  g_free (self->priv->account);
  g_free (self->priv->presence_message);

  G_OBJECT_CLASS (example_call_connection_parent_class)->finalize (object);
}

static gchar *
get_unique_connection_name (TpBaseConnection *conn)
{
  ExampleCallConnection *self = EXAMPLE_CALL_CONNECTION (conn);

  return g_strdup_printf ("%s@%p", self->priv->account, self);
}

static gchar *
example_call_normalize_contact (TpHandleRepoIface *repo,
    const gchar *id,
    gpointer context,
    GError **error)
{
  gchar *normal = NULL;

  if (example_call_protocol_check_contact_id (id, &normal, error))
    return normal;
  else
    return NULL;
}

static void
create_handle_repos (TpBaseConnection *conn,
    TpHandleRepoIface *repos[TP_NUM_ENTITY_TYPES])
{
  repos[TP_ENTITY_TYPE_CONTACT] = tp_dynamic_handle_repo_new
      (TP_ENTITY_TYPE_CONTACT, example_call_normalize_contact, NULL);
}

static GPtrArray *
create_channel_managers (TpBaseConnection *conn)
{
  ExampleCallConnection *self = EXAMPLE_CALL_CONNECTION (conn);
  GPtrArray *ret = g_ptr_array_sized_new (1);

  g_ptr_array_add (ret,
      g_object_new (EXAMPLE_TYPE_CALL_MANAGER,
        "connection", conn,
        "simulation-delay", self->priv->simulation_delay,
        NULL));

  return ret;
}

static gboolean
start_connecting (TpBaseConnection *conn,
    GError **error)
{
  ExampleCallConnection *self = EXAMPLE_CALL_CONNECTION (conn);
  TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (conn,
      TP_ENTITY_TYPE_CONTACT);
  TpHandle self_handle;

  /* In a real connection manager we'd ask the underlying implementation to
   * start connecting, then go to state CONNECTED when finished, but here
   * we can do it immediately. */

  self_handle = tp_handle_ensure (contact_repo, self->priv->account,
      NULL, error);

  if (self_handle == 0)
    return FALSE;

  tp_base_connection_set_self_handle (conn, self_handle);

  tp_base_connection_change_status (conn, TP_CONNECTION_STATUS_CONNECTED,
      TP_CONNECTION_STATUS_REASON_REQUESTED);

  return TRUE;
}

static void
shut_down (TpBaseConnection *conn)
{
  /* In a real connection manager we'd ask the underlying implementation to
   * start shutting down, then call this function when finished, but here
   * we can do it immediately. */
  tp_base_connection_finish_shutdown (conn);
}

static gboolean
status_available (TpPresenceMixin *mixin,
    guint index_)
{
  return tp_base_connection_check_connected (TP_BASE_CONNECTION (mixin), NULL);
}

static TpPresenceStatus *
get_contact_status (TpPresenceMixin *mixin,
    TpHandle contact)
{
  ExampleCallConnection *self = EXAMPLE_CALL_CONNECTION (mixin);
  TpBaseConnection *base = TP_BASE_CONNECTION (mixin);
  ExampleCallPresence presence;
  const gchar *message;

  /* we know our own status from the connection; for this example CM,
   * everyone else's status is assumed to be "available" */
  if (contact == tp_base_connection_get_self_handle (base))
    {
      presence = (self->priv->away ? EXAMPLE_CALL_PRESENCE_AWAY
          : EXAMPLE_CALL_PRESENCE_AVAILABLE);
      message = self->priv->presence_message;
    }
  else
    {
      presence = EXAMPLE_CALL_PRESENCE_AVAILABLE;
      message = NULL;
    }

  return tp_presence_status_new (presence, message);
}

static gboolean
set_own_status (TpPresenceMixin *mixin,
    const TpPresenceStatus *status,
    GError **error)
{
  ExampleCallConnection *self = EXAMPLE_CALL_CONNECTION (mixin);
  TpBaseConnection *base = TP_BASE_CONNECTION (mixin);
  GHashTable *presences;

  if (status->index == EXAMPLE_CALL_PRESENCE_AWAY)
    {
      if (self->priv->away && !tp_strdiff (status->message,
            self->priv->presence_message))
        return TRUE;

      self->priv->away = TRUE;
    }
  else
    {
      if (!self->priv->away && !tp_strdiff (status->message,
            self->priv->presence_message))
        return TRUE;

      self->priv->away = FALSE;
    }

  g_free (self->priv->presence_message);
  self->priv->presence_message = g_strdup (status->message);

  presences = g_hash_table_new_full (g_direct_hash, g_direct_equal,
      NULL, NULL);
  g_hash_table_insert (presences,
      GUINT_TO_POINTER (tp_base_connection_get_self_handle (base)),
      (gpointer) status);
  tp_presence_mixin_emit_presence_update (TP_PRESENCE_MIXIN (self), presences);
  g_hash_table_unref (presences);

  if (!self->priv->away)
    {
      g_signal_emit (self, signals[SIGNAL_AVAILABLE], 0, status->message);
    }

  return TRUE;
}

/* Must be kept in sync with ExampleCallPresence enum in header */
static const TpPresenceStatusSpec presence_statuses[] = {
      { "offline", TP_CONNECTION_PRESENCE_TYPE_OFFLINE, FALSE, FALSE },
      { "unknown", TP_CONNECTION_PRESENCE_TYPE_UNKNOWN, FALSE, FALSE },
      { "error", TP_CONNECTION_PRESENCE_TYPE_ERROR, FALSE, FALSE },
      { "away", TP_CONNECTION_PRESENCE_TYPE_AWAY, TRUE, TRUE },
      { "available", TP_CONNECTION_PRESENCE_TYPE_AVAILABLE, TRUE, TRUE },
      { NULL }
};

static const gchar *interfaces_always_present[] = {
    TP_IFACE_CONNECTION_INTERFACE_PRESENCE1,
    NULL };

const gchar * const *
example_call_connection_get_possible_interfaces (void)
{
  /* in this example CM we don't have any extra interfaces that are sometimes,
   * but not always, present */
  return interfaces_always_present;
}

static void
fill_contact_attributes (TpBaseConnection *conn,
    const gchar *dbus_interface,
    TpHandle contact,
    GVariantDict *attributes)
{
  if (tp_presence_mixin_fill_contact_attributes (TP_PRESENCE_MIXIN (conn),
        dbus_interface, contact, attributes))
    return;

  ((TpBaseConnectionClass *) example_call_connection_parent_class)->
    fill_contact_attributes (conn, dbus_interface, contact, attributes);
}

static void
init_presence (gpointer g_iface,
    gpointer iface_data)
{
  TpPresenceMixinInterface *iface = g_iface;

  iface->status_available = status_available;
  iface->get_contact_status = get_contact_status;
  iface->set_own_status = set_own_status;
  iface->statuses = presence_statuses;
}

static void
example_call_connection_class_init (
    ExampleCallConnectionClass *klass)
{
  TpBaseConnectionClass *base_class = (TpBaseConnectionClass *) klass;
  GObjectClass *object_class = (GObjectClass *) klass;
  GParamSpec *param_spec;

  object_class->get_property = get_property;
  object_class->set_property = set_property;
  object_class->constructed = constructed;
  object_class->finalize = finalize;
  g_type_class_add_private (klass,
      sizeof (ExampleCallConnectionPrivate));

  base_class->create_handle_repos = create_handle_repos;
  base_class->get_unique_connection_name = get_unique_connection_name;
  base_class->create_channel_managers = create_channel_managers;
  base_class->start_connecting = start_connecting;
  base_class->shut_down = shut_down;
  base_class->fill_contact_attributes = fill_contact_attributes;

  param_spec = g_param_spec_string ("account", "Account name",
      "The username of this user", NULL,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_ACCOUNT, param_spec);

  param_spec = g_param_spec_uint ("simulation-delay", "Simulation delay",
      "Delay between simulated network events",
      0, G_MAXUINT32, 1000,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_SIMULATION_DELAY,
      param_spec);

  /* Used in the call manager, to simulate an incoming call when we become
   * available */
  signals[SIGNAL_AVAILABLE] = g_signal_new ("available",
      G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST, 0, NULL, NULL,
      NULL, G_TYPE_NONE, 1, G_TYPE_STRING);
}
