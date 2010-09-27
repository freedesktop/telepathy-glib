/*
 * Object representing a connection on a Stream Tube
 *
 * Copyright © 2010 Collabora Ltd.
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

/**
 * SECTION:stream-tube-connection
 * @title: TpStreamTubeConnection
 * @short_description: a connection on a Stream Tube
 *
 * Object used to represent a connection on a #TpStreamTubeChannel.
 */

/**
 * TpStreamTubeConnection:
 *
 * Data structure representing a connection on a #TpStreamTubeChannel.
 *
 * Since: 0.13.UNRELEASED
 */

/**
 * TpStreamTubeConnectionClass:
 *
 * The class of a #TpStreamTubeConnection.
 *
 * Since: 0.13.UNRELEASED
 */

#include "telepathy-glib/stream-tube-connection-internal.h"
#include "telepathy-glib/stream-tube-connection.h"

#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL
#include "telepathy-glib/debug-internal.h"

struct _TpStreamTubeConnectionClass {
    /*<private>*/
    GObjectClass parent_class;
};

G_DEFINE_TYPE(TpStreamTubeConnection, tp_stream_tube_connection,
    G_TYPE_OBJECT)

enum {
    PROP_CONNECTION = 1,
    PROP_CONTACT,
    N_PROPS
};

struct _TpStreamTubeConnectionPrivate
{
  GSocketConnection *connection;
  TpContact *contact;
};

static void
tp_stream_tube_connection_init (TpStreamTubeConnection *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_STREAM_TUBE_CONNECTION, TpStreamTubeConnectionPrivate);
}

static void
tp_stream_tube_connection_dispose (GObject *object)
{
  TpStreamTubeConnection *self = TP_STREAM_TUBE_CONNECTION (object);
  void (*dispose) (GObject *) =
    G_OBJECT_CLASS (tp_stream_tube_connection_parent_class)->dispose;

  tp_clear_object (&self->priv->connection);
  tp_clear_object (&self->priv->contact);

  if (dispose != NULL)
    dispose (object);
}

static void
tp_stream_tube_connection_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  TpStreamTubeConnection *self = TP_STREAM_TUBE_CONNECTION (object);

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_value_set_object (value, self->priv->connection);
        break;
      case PROP_CONTACT:
        g_value_set_object (value, self->priv->contact);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_stream_tube_connection_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  TpStreamTubeConnection *self = TP_STREAM_TUBE_CONNECTION (object);

  switch (property_id)
    {
      case PROP_CONNECTION:
        g_assert (self->priv->connection == NULL); /* construct only */
        self->priv->connection = g_value_dup_object (value);
        break;
      case PROP_CONTACT:
        g_assert (self->priv->contact == NULL); /* construct only */
        self->priv->contact = g_value_dup_object (value);
        break;
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
  }
}

static void
tp_stream_tube_connection_constructed (GObject *object)
{
  TpStreamTubeConnection *self = TP_STREAM_TUBE_CONNECTION (object);
  void (*chain_up) (GObject *) =
    ((GObjectClass *) tp_stream_tube_connection_parent_class)->constructed;

  if (chain_up != NULL)
    chain_up (object);

  g_assert (self->priv->connection != NULL);
}

static void
tp_stream_tube_connection_class_init (TpStreamTubeConnectionClass *cls)
{
  GObjectClass *object_class = G_OBJECT_CLASS (cls);
  GParamSpec *param_spec;

  g_type_class_add_private (cls, sizeof (TpStreamTubeConnectionPrivate));

  object_class->get_property = tp_stream_tube_connection_get_property;
  object_class->set_property = tp_stream_tube_connection_set_property;
  object_class->constructed = tp_stream_tube_connection_constructed;
  object_class->dispose = tp_stream_tube_connection_dispose;

  /**
   * TpStreamTubeConnection:connection:
   *
   * The #GSocketConnection used to transfer data through this connection.
   * Read-only except during construction.
   *
   * This property can't be %NULL.
   *
   * Since: 0.13.UNRELEASED
   */
  param_spec = g_param_spec_object ("connection", "GSocketConnection",
      "GSocketConnection used to transfer data",
      G_TYPE_SOCKET_CONNECTION,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION,
      param_spec);

  /**
   * TpStreamTubeConnection:contact:
   *
   * The #TpContact with who we are exchanging data through this tube, or
   * %NULL if we can't safely identify the contact.
   *
   * Since: 0.13.UNRELEASED
   */
  param_spec = g_param_spec_object ("contact", "TpContact",
      "The TpContact of the connection",
      TP_TYPE_CONTACT,
      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONTACT, param_spec);
}

TpStreamTubeConnection * _tp_stream_tube_connection_new (
    GSocketConnection *connection)
{
  return g_object_new (TP_TYPE_STREAM_TUBE_CONNECTION,
      "connection", connection,
      NULL);
}

/**
 * tp_stream_tube_connection_get_connection: (skip)
 * @self: a #TpStreamTubeConnection
 *
 * Return the #TpStreamTubeConnection:connection property
 *
 * Returns: (transfer none): the value of #TpStreamTubeConnection:connection
 *
 * Since: 0.13.UNRELEASED
 */
GSocketConnection *
tp_stream_tube_connection_get_connection (TpStreamTubeConnection *self)
{
  return self->priv->connection;
}

/**
 * tp_stream_tube_connection_get_contact: (skip)
 * @self: a #TpStreamTubeConnection
 *
 * Return the #TpStreamTubeConnection:contact property
 *
 * Returns: (transfer none): the value of #TpStreamTubeConnection:contact
 *
 * Since: 0.13.UNRELEASED
 */
TpContact *
tp_stream_tube_connection_get_contact (TpStreamTubeConnection *self)
{
  return self->priv->contact;
}

void
_tp_stream_tube_connection_set_contact (TpStreamTubeConnection *self,
    TpContact *contact)
{
  g_assert (self->priv->contact == NULL);

  self->priv->contact = g_object_ref (contact);
  g_object_notify (G_OBJECT (self), "contact");
}
