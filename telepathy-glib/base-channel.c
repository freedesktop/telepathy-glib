/*
 * base-channel.c - base class for Channel implementations
 *
 * Copyright (C) 2009 Collabora Ltd.
 * Copyright (C) 2009 Nokia Corporation
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
 * SECTION:base-channel
 * @title: TpBaseChannel
 * @short_description: base class for #TpExportableChannel implementations
 * @see_also: #TpSvcChannel
 *
 * This base class makes it easier to write #TpExportableChannel
 * implementations by implementing some of its properties, and defining other
 * relevant properties.
 *
 * Subclasses should fill in #TpBaseChannelClass.channel_type,
 * #TpBaseChannelClass.target_handle_type and #TpBaseChannelClass.interfaces,
 * and implement the #TpBaseChannelClass.close virtual function.
 *
 * If the channel type and/or interfaces being implemented define immutable
 * D-Bus properties besides those on the Channel interface, the subclass should
 * implement the #TpBaseChannelClass.fill_immutable_properties virtual function.
 *
 * If the #TpExportableChannel:object-path property is not set at construct
 * time, the #TpBaseChannelClass.get_object_path_suffix virtual function will
 * be called to determine the channel's path, whose default implementation
 * simply generates a unique path based on the object's address in memory.
 *
 * Since: 0.11.UNRELEASED
 */

/**
 * TpBaseChannel:
 *
 * A base class for channel implementations
 */

/**
 * TpBaseChannelClass:
 * @dbus_props_class: The class structure for the DBus properties mixin
 * @channel_type: The type of channel that instances of this class represent
 * (e.g. #TP_IFACE_CHANNEL_TYPE_TEXT)
 * @target_handle_type: The type of handle that is the target of channels of
 * this type
 * @interfaces: Extra interfaces provided by this channel (this SHOULD NOT
 * include the channel type and interface itself)
 * @close: A virtual function called to close the channel. Implementations
 *  should generally call either tp_base_channel_destroyed() if the channel is
 *  really closed as a result, or tp_base_channel_reopened() if the channel
 *  will be re-spawned (for instance, due to unacknowledged messages). Note
 *  that channels that support re-spawning must also implement
 *  #TpSvcChannelInterfaceDestroyable.
 * @fill_immutable_properties: A virtual function called to add custom
 * properties to the DBus properties hash.  Implementations must chain up to the
 * parent class implementation and call
 * tp_dbus_properties_mixin_fill_properties_hash() on the supplied hash table
 * @get_object_path_suffix: Returns a string that will be appended to the
 * Connection objects's object path to get the Channel's object path.  This
 * function will only be called as a fallback if the
 * #TpExportableChannel:object-path property is not set.  The default
 * implementation simply generates a unique path based on the object's address
 * in memory.  The returned string will be freed automatically.
 *
 * The class structure for #TpBaseChannel
 */

/**
 * TpBaseChannelCloseFunc:
 * @chan: a channel
 *
 * Signature of an implementation of the #TpBaseChannelClass.close virtual
 * function.
 */

/**
 * TpBaseChannelFillPropertiesFunc:
 * @chan: a channel
 * @properties: a dictionary of @chan's immutable properties, which the
 *  implementation may add to using
 *  tp_dbus_properties_mixin_fill_properties_hash()
 *
 * Signature of an implementation of the
 * #TpBaseChannelClass.fill_immutable_properties
 * virtual function. A typical implementation, for a channel implementing
 * #TpSvcChannelTypeContactSearch, would be:
 *
 * |[
 * static void
 * my_search_channel_fill_properties (
 *     TpBaseChannel *chan,
 *     GHashTable *properties)
 * {
 *   TpBaseChannelClass *klass = TP_BASE_CHANNEL_CLASS (my_search_channel_parent_class);
 *
 *   klass->fill_immutable_properties (chan, properties);
 *
 *   tp_dbus_properties_mixin_fill_properties_hash (
 *       G_OBJECT (chan), properties,
 *       TP_IFACE_CHANNEL_TYPE_CONTACT_SEARCH, "Limit",
 *       TP_IFACE_CHANNEL_TYPE_CONTACT_SEARCH, "AvailableSearchKeys",
 *       TP_IFACE_CHANNEL_TYPE_CONTACT_SEARCH, "Server",
 *       NULL);
 * }
 * ]|
 *
 * Note that the SearchState property is <emphasis>not</emphasis> added to
 * @properties, since only immutable properties (whose value cannot change over
 * the lifetime of @chan) should be included.
 */

/**
 * TpBaseChannelGetPathFunc:
 * @chan: a channel
 *
 * Signature of an implementation of the
 * #TpBaseChannelClass.get_object_path_suffix virtual function.
 *
 * Returns: (transfer full): a string that will be appended to the Connection
 * objects's object path to get the Channel's object path.
 */

#include "config.h"
#include "base-channel.h"

#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/channel-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/exportable-channel.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/svc-channel.h>
#include <telepathy-glib/svc-generic.h>
#include <telepathy-glib/debug-internal.h>
#include <telepathy-glib/util.h>

#define DEBUG_FLAG TP_DEBUG_CHANNEL

#include "debug-internal.h"

enum
{
  PROP_OBJECT_PATH = 1,
  PROP_CHANNEL_TYPE,
  PROP_HANDLE_TYPE,
  PROP_HANDLE,
  PROP_INITIATOR_HANDLE,
  PROP_INITIATOR_ID,
  PROP_TARGET_ID,
  PROP_REQUESTED,
  PROP_CONNECTION,
  PROP_INTERFACES,
  PROP_CHANNEL_DESTROYED,
  PROP_CHANNEL_PROPERTIES,
  LAST_PROPERTY
};

struct _TpBaseChannelPrivate
{
  TpBaseConnection *conn;

  char *object_path;

  TpHandle target;
  TpHandle initiator;

  gboolean requested;
  gboolean destroyed;
  gboolean registered;

  gboolean dispose_has_run;
};

static void channel_iface_init (gpointer g_iface, gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE (TpBaseChannel, tp_base_channel,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_DBUS_PROPERTIES,
      tp_dbus_properties_mixin_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_SVC_CHANNEL, channel_iface_init);
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_IFACE, NULL);
    G_IMPLEMENT_INTERFACE (TP_TYPE_EXPORTABLE_CHANNEL, NULL);
    )

/**
 * tp_base_channel_register:
 * @chan: a channel
 *
 * Make the channel appear on the bus.  #TpExportableChannel:object-path must have been set
 * to a valid path, which must not already be in use as another object's path.
 */
void
tp_base_channel_register (TpBaseChannel *chan)
{
  TpDBusDaemon *bus = tp_base_connection_get_dbus_daemon (chan->priv->conn);

  g_assert (chan->priv->object_path != NULL);
  g_return_if_fail (!chan->priv->registered);

  tp_dbus_daemon_register_object (bus, chan->priv->object_path, chan);
  chan->priv->registered = TRUE;
}

/**
 * tp_base_channel_destroyed:
 * @chan: a channel
 *
 * Called by subclasses to indicate that this channel was destroyed and can be
 * removed from the bus.  The "Closed" signal will be emitted and the
 * #TpExportableChannel:channel-destroyed property will be set.
 */
void
tp_base_channel_destroyed (TpBaseChannel *chan)
{
  TpDBusDaemon *bus = tp_base_connection_get_dbus_daemon (chan->priv->conn);

  /* Take a ref to ourself: the 'closed' handler might drop its reference on us.
   */
  g_object_ref (chan);

  chan->priv->destroyed = TRUE;
  tp_svc_channel_emit_closed (chan);

  if (chan->priv->registered)
    {
      tp_dbus_daemon_unregister_object (bus, chan);
      chan->priv->registered = FALSE;
    }

  g_object_unref (chan);
}

/**
 * tp_base_channel_reopened:
 * @chan: a channel
 * @initiator: the handle of the contact that re-opened the channel
 *
 * Called by subclasses to indicate that this channel was closed but was
 * re-opened due to pending messages.  The "Closed" signal will be emitted, but
 * the #TpExportableChannel:channel-destroyed property will not be set.  The
 * channel's #TpBaseChannel:initiator-handle property will be set to
 * @initiator, and the #TpBaseChannel:requested property will be set to FALSE.
 */
void
tp_base_channel_reopened (TpBaseChannel *chan, TpHandle initiator)
{
  TpBaseChannelPrivate *priv = chan->priv;

  /* Take a ref to ourself: the 'closed' handler might drop its reference on us.
   */
  g_object_ref (chan);

  if (priv->initiator != initiator)
    {
      TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
          priv->conn, TP_HANDLE_TYPE_CONTACT);
      TpHandle old_initiator = priv->initiator;

      if (initiator != 0)
        tp_handle_ref (contact_repo, initiator);

      priv->initiator = initiator;

      if (old_initiator != 0)
        tp_handle_unref (contact_repo, old_initiator);
    }

  chan->priv->requested = FALSE;

  tp_svc_channel_emit_closed (chan);

  g_object_unref (chan);
}

/**
 * tp_base_channel_close:
 * @chan: a channel
 *
 * Asks @chan to close, just as if the Close D-Bus method had been called. If
 * #TpExportableChannel:channel-destroyed is TRUE, this is a no-op.
 *
 * Note that, depending on the subclass's implementation of
 * #TpBaseChannelClass.close and internal behaviour, this may or may not be a
 * suitable method to use during connection teardown. For instance, if the
 * channel may respawn when Close is called, an equivalent of the Destroy D-Bus
 * method would be more appropriate during teardown, since the intention is to
 * forcibly terminate all channels.
 */
void
tp_base_channel_close (TpBaseChannel *chan)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (chan);

  if (!tp_base_channel_is_destroyed (chan))
    klass->close (chan);
}

/**
 * tp_base_channel_get_object_path:
 * @chan: a channel
 *
 * Returns @chan's object path, as a shortcut for retrieving the
 * #TpChannelIface:object-path property.
 *
 * Returns: @chan's object path
 */
const gchar *
tp_base_channel_get_object_path (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), NULL);

  return chan->priv->object_path;
}

/**
 * tp_base_channel_get_connection:
 * @chan: a channel
 *
 * Returns the connection to which @chan is attached, as a shortcut for
 * retrieving the #TpBaseChannel:connection property.
 *
 * Returns: (tranfer none): the connection to which @chan is attached.
 */
TpBaseConnection *
tp_base_channel_get_connection (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), NULL);

  return chan->priv->conn;
}

/**
 * tp_base_channel_get_target_handle:
 * @chan: a channel
 *
 * Returns the target handle of @chan (without a reference), which will be 0
 * if #TpBaseChannelClass.target_handle_type is #TP_HANDLE_TYPE_NONE for this
 * class, and non-zero otherwise. This is a shortcut for retrieving the
 * #TpChannelIface:handle property. The reference count of the handle
 * is not increased; you should use tp_handle_ref() if you want to keep a hold
 * of it.
 *
 * Returns: the target handle of @chan
 */
TpHandle
tp_base_channel_get_target_handle (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), 0);

  return chan->priv->target;
}

/**
 * tp_base_channel_get_initiator:
 * @chan: a channel
 *
 * Returns the initiator handle of @chan, as a shortcut for retrieving the
 * #TpBaseChannel:initiator-handle property. The reference count of the handle
 * is not increased; you should use tp_handle_ref() if you want to keep a hold
 * of it.
 *
 * Returns: the initiator handle of @chan
 */
TpHandle
tp_base_channel_get_initiator (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), 0);

  return chan->priv->initiator;
}

/**
 * tp_base_channel_is_requested:
 * @chan: a channel
 *
 * Returns whether or not @chan was requested, as a shortcut for retrieving the
 * #TpBaseChannel:requested property.
 *
 * Returns: whether or not @chan was requested.
 */
gboolean
tp_base_channel_is_requested (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), FALSE);

  return chan->priv->requested;
}

/**
 * tp_base_channel_is_registered:
 * @chan: a channel
 *
 * Returns whether or not @chan is visible on the bus; that is, whether
 * tp_base_channel_register() has been called and tp_base_channel_destroyed()
 * has not been called.
 *
 * Returns: TRUE if @chan is visible on the bus
 */
gboolean
tp_base_channel_is_registered (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), FALSE);

  return chan->priv->registered;
}

/**
 * tp_base_channel_is_destroyed:
 * @chan: a channel
 *
 * Returns the value of the #TpExportableChannel:channel-destroyed property,
 * which is TRUE if tp_base_channel_destroyed() has been called (and thus the
 * channel has been removed from the bus).
 *
 * Returns: TRUE if tp_base_channel_destroyed() has been called.
 */
gboolean
tp_base_channel_is_destroyed (TpBaseChannel *chan)
{
  g_return_val_if_fail (TP_IS_BASE_CHANNEL (chan), FALSE);

  return chan->priv->destroyed;
}


/*
 * tp_base_channel_fill_basic_immutable_properties:
 *
 * Specifies the immutable properties supported for this Channel object, by
 * using tp_dbus_properties_mixin_fill_properties_hash().
 */
static void
tp_base_channel_fill_basic_immutable_properties (TpBaseChannel *chan, GHashTable *properties)
{
  tp_dbus_properties_mixin_fill_properties_hash (G_OBJECT (chan),
      properties,
      TP_IFACE_CHANNEL, "ChannelType",
      TP_IFACE_CHANNEL, "TargetHandleType",
      TP_IFACE_CHANNEL, "TargetHandle",
      TP_IFACE_CHANNEL, "TargetID",
      TP_IFACE_CHANNEL, "InitiatorHandle",
      TP_IFACE_CHANNEL, "InitiatorID",
      TP_IFACE_CHANNEL, "Requested",
      TP_IFACE_CHANNEL, "Interfaces",
      NULL);
}

static gchar *
tp_base_channel_get_basic_object_path_suffix (TpBaseChannel *self)
{
  gchar *obj_path = g_strdup_printf ("channel%p", self);
  gchar *escaped = tp_escape_as_identifier (obj_path);

  g_free (obj_path);

  return escaped;
}

static void
tp_base_channel_init (TpBaseChannel *self)
{
  TpBaseChannelPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CHANNEL, TpBaseChannelPrivate);

  self->priv = priv;

}

static void
tp_base_channel_constructed (GObject *object)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (object);
  GObjectClass *parent_class = tp_base_channel_parent_class;
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);
  TpBaseConnection *conn = chan->priv->conn;
  TpHandleRepoIface *handles;

  if (parent_class->constructed != NULL)
    parent_class->constructed (object);

  if (klass->target_handle_type != TP_HANDLE_TYPE_NONE)
    {
      handles = tp_base_connection_get_handles (conn, klass->target_handle_type);
      g_assert (handles != NULL);
      g_assert (chan->priv->target != 0);
      tp_handle_ref (handles, chan->priv->target);
    }

  if (chan->priv->initiator != 0)
    {
      handles = tp_base_connection_get_handles (conn, TP_HANDLE_TYPE_CONTACT);
      g_assert (handles != NULL);
      tp_handle_ref (handles, chan->priv->initiator);
    }

  if (chan->priv->object_path == NULL)
    {
      gchar *base_path = klass->get_object_path_suffix (chan);

      g_assert (base_path != NULL);
      g_assert (*base_path != '\0');

      chan->priv->object_path = g_strdup_printf ("%s/%s",
          conn->object_path, base_path);
      g_free (base_path);
    }
}

static void
tp_base_channel_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (chan);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_value_set_string (value, chan->priv->object_path);
      break;
    case PROP_CHANNEL_TYPE:
      g_value_set_static_string (value, klass->channel_type);
      break;
    case PROP_HANDLE_TYPE:
      g_value_set_uint (value, klass->target_handle_type);
      break;
    case PROP_HANDLE:
      g_value_set_uint (value, chan->priv->target);
      break;
    case PROP_TARGET_ID:
      if (chan->priv->target != 0)
        {
          TpHandleRepoIface *repo = tp_base_connection_get_handles (
              chan->priv->conn, klass->target_handle_type);

          g_value_set_string (value, tp_handle_inspect (repo, chan->priv->target));
        }
      else
        {
          g_value_set_static_string (value, "");
        }
      break;
    case PROP_INITIATOR_HANDLE:
      g_value_set_uint (value, chan->priv->initiator);
      break;
    case PROP_INITIATOR_ID:
      if (chan->priv->initiator != 0)
        {
          TpHandleRepoIface *repo = tp_base_connection_get_handles (
              chan->priv->conn, TP_HANDLE_TYPE_CONTACT);

          g_assert (chan->priv->initiator != 0);
          g_value_set_string (value, tp_handle_inspect (repo, chan->priv->initiator));
        }
      else
        {
          g_value_set_static_string (value, "");
        }
      break;
    case PROP_REQUESTED:
      g_value_set_boolean (value, (chan->priv->requested));
      break;
    case PROP_CONNECTION:
      g_value_set_object (value, chan->priv->conn);
      break;
    case PROP_INTERFACES:
      g_value_set_boxed (value, klass->interfaces);
      break;
    case PROP_CHANNEL_DESTROYED:
      g_value_set_boolean (value, chan->priv->destroyed);
      break;
    case PROP_CHANNEL_PROPERTIES:
        {
          /* create an empty properties hash for subclasses to fill */
          GHashTable *properties =
            tp_dbus_properties_mixin_make_properties_hash (G_OBJECT (chan), NULL, NULL, NULL);

          if (klass->fill_immutable_properties)
            klass->fill_immutable_properties (chan, properties);

          g_value_take_boxed (value, properties);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_base_channel_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);

  switch (property_id) {
    case PROP_OBJECT_PATH:
      g_assert (chan->priv->object_path == NULL);
      chan->priv->object_path = g_value_dup_string (value);
      break;
    case PROP_HANDLE:
      /* we don't ref it here because we don't necessarily have access to the
       * contact repo yet - instead we ref it in constructed.
       */
      chan->priv->target = g_value_get_uint (value);
      break;
    case PROP_INITIATOR_HANDLE:
      /* similarly we can't ref this yet */
      chan->priv->initiator = g_value_get_uint (value);
      break;
    case PROP_HANDLE_TYPE:
    case PROP_CHANNEL_TYPE:
      /* these properties are writable in the interface, but not actually
       * meaningfully changeable on this channel, so we do nothing */
      break;
    case PROP_CONNECTION:
      chan->priv->conn = g_value_dup_object (value);
      break;
    case PROP_REQUESTED:
      chan->priv->requested = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
tp_base_channel_dispose (GObject *object)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (chan);
  TpBaseChannelPrivate *priv = chan->priv;
  TpHandleRepoIface *handles;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  if (!priv->destroyed)
    {
      tp_base_channel_destroyed (chan);
    }

  if (priv->target != 0)
    {
      handles = tp_base_connection_get_handles (priv->conn,
                                                klass->target_handle_type);
      tp_handle_unref (handles, priv->target);
      priv->target = 0;
    }

  if (priv->initiator != 0)
    {
      handles = tp_base_connection_get_handles (priv->conn,
                                                TP_HANDLE_TYPE_CONTACT);
      tp_handle_unref (handles, priv->initiator);
      priv->initiator = 0;
    }

  tp_clear_object (&priv->conn);

  if (G_OBJECT_CLASS (tp_base_channel_parent_class)->dispose)
    G_OBJECT_CLASS (tp_base_channel_parent_class)->dispose (object);
}

static void
tp_base_channel_finalize (GObject *object)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (object);

  g_free (chan->priv->object_path);

  G_OBJECT_CLASS (tp_base_channel_parent_class)->finalize (object);
}

static void
tp_base_channel_class_init (TpBaseChannelClass *tp_base_channel_class)
{
  static TpDBusPropertiesMixinPropImpl channel_props[] = {
      { "TargetHandleType", "handle-type", NULL },
      { "TargetHandle", "handle", NULL },
      { "TargetID", "target-id", NULL },
      { "ChannelType", "channel-type", NULL },
      { "Interfaces", "interfaces", NULL },
      { "Requested", "requested", NULL },
      { "InitiatorHandle", "initiator-handle", NULL },
      { "InitiatorID", "initiator-id", NULL },
      { NULL }
  };
  static TpDBusPropertiesMixinIfaceImpl prop_interfaces[] = {
      { TP_IFACE_CHANNEL,
        tp_dbus_properties_mixin_getter_gobject_properties,
        NULL,
        channel_props,
      },
      { NULL }
  };
  GObjectClass *object_class = G_OBJECT_CLASS (tp_base_channel_class);
  GParamSpec *param_spec;

  g_type_class_add_private (tp_base_channel_class,
      sizeof (TpBaseChannelPrivate));

  object_class->constructed = tp_base_channel_constructed;

  object_class->get_property = tp_base_channel_get_property;
  object_class->set_property = tp_base_channel_set_property;

  object_class->dispose = tp_base_channel_dispose;
  object_class->finalize = tp_base_channel_finalize;

  g_object_class_override_property (object_class, PROP_OBJECT_PATH,
      "object-path");
  g_object_class_override_property (object_class, PROP_CHANNEL_TYPE,
      "channel-type");
  g_object_class_override_property (object_class, PROP_HANDLE_TYPE,
      "handle-type");
  g_object_class_override_property (object_class, PROP_HANDLE, "handle");
  g_object_class_override_property (object_class, PROP_CHANNEL_DESTROYED,
      "channel-destroyed");
  g_object_class_override_property (object_class, PROP_CHANNEL_PROPERTIES,
      "channel-properties");

  param_spec = g_param_spec_object ("connection", "TpBaseConnection object",
      "Connection object that owns this channel.",
      TP_TYPE_BASE_CONNECTION,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

  param_spec = g_param_spec_boxed ("interfaces", "Extra D-Bus interfaces",
      "Additional Channel.Interface.* interfaces",
      G_TYPE_STRV,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INTERFACES, param_spec);

  param_spec = g_param_spec_string ("target-id", "Target's identifier",
      "The string obtained by inspecting the target handle",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_TARGET_ID, param_spec);

  param_spec = g_param_spec_boolean ("requested", "Requested?",
      "True if this channel was requested by the local user",
      FALSE,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_REQUESTED, param_spec);

  param_spec = g_param_spec_uint ("initiator-handle", "Initiator's handle",
      "The contact who initiated the channel",
      0, G_MAXUINT32, 0,
      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIATOR_HANDLE,
      param_spec);

  param_spec = g_param_spec_string ("initiator-id", "Initiator's bare JID",
      "The string obtained by inspecting the initiator-handle",
      NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_INITIATOR_ID,
      param_spec);

  tp_base_channel_class->dbus_props_class.interfaces = prop_interfaces;
  tp_dbus_properties_mixin_class_init (object_class,
      G_STRUCT_OFFSET (TpBaseChannelClass, dbus_props_class));
  tp_base_channel_class->fill_immutable_properties =
      tp_base_channel_fill_basic_immutable_properties;
  tp_base_channel_class->get_object_path_suffix =
      tp_base_channel_get_basic_object_path_suffix;
}

static void
tp_base_channel_get_channel_type (TpSvcChannel *iface,
                                  DBusGMethodInvocation *context)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (iface);

  tp_svc_channel_return_from_get_channel_type (context, klass->channel_type);
}

static void
tp_base_channel_get_handle (TpSvcChannel *iface,
                            DBusGMethodInvocation *context)
{
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (iface);
  TpBaseChannel *chan = TP_BASE_CHANNEL (iface);

  tp_svc_channel_return_from_get_handle (context, klass->target_handle_type,
      chan->priv->target);
}

static void
tp_base_channel_get_interfaces (TpSvcChannel *iface,
                                DBusGMethodInvocation *context)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (iface);
  TpBaseChannelClass *klass = TP_BASE_CHANNEL_GET_CLASS (chan);

  tp_svc_channel_return_from_get_interfaces (context, klass->interfaces);
}

static void
tp_base_channel_close_dbus (
    TpSvcChannel *iface,
    DBusGMethodInvocation *context)
{
  TpBaseChannel *chan = TP_BASE_CHANNEL (iface);

  if (DEBUGGING)
    {
      gchar *caller = dbus_g_method_get_sender (context);

      DEBUG ("called by %s", caller);
      g_free (caller);
    }

  tp_base_channel_close (chan);
  tp_svc_channel_return_from_close (context);
}

static void
channel_iface_init (gpointer g_iface,
                    gpointer iface_data)
{
  TpSvcChannelClass *klass = (TpSvcChannelClass *) g_iface;

#define IMPLEMENT(x) tp_svc_channel_implement_##x (\
    klass, tp_base_channel_##x)
  IMPLEMENT(get_channel_type);
  IMPLEMENT(get_handle);
  IMPLEMENT(get_interfaces);
#undef IMPLEMENT

  tp_svc_channel_implement_close (klass, tp_base_channel_close_dbus);
}