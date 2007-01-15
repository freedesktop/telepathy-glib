/*
 * base-connection.c - Source for TpBaseConnection
 *
 * Copyright (C) 2005, 2006, 2007 Collabora Ltd.
 * Copyright (C) 2005, 2006, 2007 Nokia Corporation
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

#include <telepathy-glib/base-connection.h>

#include <string.h>

#include <dbus/dbus-glib-lowlevel.h>

#include <telepathy-glib/channel-factory-iface.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/enums.h>

#define DEBUG_FLAG TP_DEBUG_CONNECTION
#include "debug.h"
#include "_gen/signals-marshal.h"

#define BUS_NAME_BASE    "org.freedesktop.Telepathy.Connection"
#define OBJECT_PATH_BASE "/org/freedesktop/Telepathy/Connection"

G_DEFINE_ABSTRACT_TYPE(TpBaseConnection, tp_base_connection, G_TYPE_OBJECT)

#define TP_BASE_CONNECTION_GET_PRIVATE(obj) \
    ((TpBaseConnectionPrivate *)obj->priv)

#define TP_CHANNEL_LIST_ENTRY_TYPE (dbus_g_type_get_struct ("GValueArray", \
      DBUS_TYPE_G_OBJECT_PATH, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, \
      G_TYPE_INVALID))

#define ERROR_IF_NOT_CONNECTED(CONN, ERROR) \
  if ((CONN)->status != TP_CONNECTION_STATUS_CONNECTED) \
    { \
      DEBUG ("rejected request as disconnected"); \
      g_set_error (ERROR, TP_ERRORS, TP_ERROR_NOT_AVAILABLE, \
          "Connection is disconnected"); \
      return FALSE; \
    }

#define ERROR_IF_NOT_CONNECTED_ASYNC(CONN, ERROR, CONTEXT) \
  if ((CONN)->status != TP_CONNECTION_STATUS_CONNECTED) \
    { \
      DEBUG ("rejected request as disconnected"); \
      (ERROR) = g_error_new (TP_ERRORS, TP_ERROR_NOT_AVAILABLE, \
          "Connection is disconnected"); \
      dbus_g_method_return_error ((CONTEXT), (ERROR)); \
      g_error_free ((ERROR)); \
      return; \
    }

enum
{
  NEW_CHANNEL,
  N_SIGNALS
};

static guint signals[N_SIGNALS] = {0};

typedef struct _ChannelRequest ChannelRequest;

struct _ChannelRequest
{
  DBusGMethodInvocation *context;
  gchar *channel_type;
  guint handle_type;
  guint handle;
  gboolean suppress_handler;
};

static ChannelRequest *
channel_request_new (DBusGMethodInvocation *context,
                     const char *channel_type,
                     guint handle_type,
                     guint handle,
                     gboolean suppress_handler)
{
  ChannelRequest *ret;

  g_assert (NULL != context);
  g_assert (NULL != channel_type);

  ret = g_new0 (ChannelRequest, 1);
  ret->context = context;
  ret->channel_type = g_strdup (channel_type);
  ret->handle_type = handle_type;
  ret->handle = handle;
  ret->suppress_handler = suppress_handler;

  return ret;
}

static void
channel_request_free (ChannelRequest *request)
{
  g_assert (NULL == request->context);
  g_free (request->channel_type);
  g_free (request);
}

static void
channel_request_cancel (gpointer data, gpointer user_data)
{
  ChannelRequest *request = (ChannelRequest *) data;
  GError *error;

  DEBUG ("cancelling request for %s/%d/%d", request->channel_type, request->handle_type, request->handle);

  error = g_error_new (TP_ERRORS, TP_ERROR_DISCONNECTED, "unable to "
      "service this channel request, we're disconnecting!");

  dbus_g_method_return_error (request->context, error);
  request->context = NULL;

  g_error_free (error);
  channel_request_free (request);
}

typedef struct _TpBaseConnectionPrivate
{
  /* if TRUE, the object has gone away */
  gboolean dispose_has_run;
  /* array of (TpChannelFactoryIface *) */
  GPtrArray *channel_factories;
  /* array of (ChannelRequest *) */
  GPtrArray *channel_requests;
  /* if TRUE, suppress the next handler. This is strange and relies on
   * the CM being non-reentrant - FIXME */
  gboolean suppress_next_handler;
} TpBaseConnectionPrivate;

static void
tp_base_connection_dispose (GObject *object)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (object);
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (self);
  DBusGProxy *bus_proxy = tp_get_bus_proxy ();
  guint i;

  if (priv->dispose_has_run)
    return;

  priv->dispose_has_run = TRUE;

  g_assert ((self->status == TP_CONNECTION_STATUS_DISCONNECTED) ||
            (self->status == TP_INTERNAL_CONNECTION_STATUS_NEW));
  g_assert (self->self_handle == 0);

  if (NULL != self->bus_name)
    {
      dbus_g_proxy_call_no_reply (bus_proxy, "ReleaseName",
                                  G_TYPE_STRING, self->bus_name,
                                  G_TYPE_INVALID);
    }

  for (i = 0; i <= LAST_TP_HANDLE_TYPE; i++)
    {
      if (self->handles[i])
        g_object_unref((GObject *)self->handles[i]);
        self->handles[i] = NULL;
    }

  g_ptr_array_foreach (priv->channel_factories, (GFunc) g_object_unref, NULL);
  g_ptr_array_free (priv->channel_factories, TRUE);
  priv->channel_factories = NULL;

  if (priv->channel_requests)
    {
      g_assert (priv->channel_requests->len == 0);
      g_ptr_array_free (priv->channel_requests, TRUE);
      priv->channel_requests = NULL;
    }

  if (G_OBJECT_CLASS (tp_base_connection_parent_class)->dispose)
    G_OBJECT_CLASS (tp_base_connection_parent_class)->dispose (object);
}

static void
tp_base_connection_finalize (GObject *object)
{
  TpBaseConnection *self = TP_BASE_CONNECTION (object);

  g_free (self->bus_name);
  g_free (self->object_path);

  tp_properties_mixin_finalize (object);

  G_OBJECT_CLASS (tp_base_connection_parent_class)->finalize (object);
}

static void
tp_base_connection_class_init (TpBaseConnectionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TpBaseConnectionPrivate));
  object_class->dispose = tp_base_connection_dispose;
  object_class->finalize = tp_base_connection_finalize;

  signals[NEW_CHANNEL] =
    g_signal_new ("new-channel",
                  G_OBJECT_CLASS_TYPE (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                  0,
                  NULL, NULL,
                  _tp_marshal_VOID__STRING_STRING_UINT_UINT_BOOLEAN,
                  G_TYPE_NONE, 5, DBUS_TYPE_G_OBJECT_PATH, G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_BOOLEAN);
}

static GPtrArray *
find_matching_channel_requests (TpBaseConnection *conn,
                                const gchar *channel_type,
                                guint handle_type,
                                guint handle,
                                gboolean *suppress_handler)
{
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (conn);
  GPtrArray *requests;
  guint i;

  requests = g_ptr_array_sized_new (1);

  for (i = 0; i < priv->channel_requests->len; i++)
    {
      ChannelRequest *request = g_ptr_array_index (priv->channel_requests, i);

      if (0 != strcmp (request->channel_type, channel_type))
        continue;

      if (handle_type != request->handle_type)
        continue;

      if (handle != request->handle)
        continue;

      if (request->suppress_handler && suppress_handler)
        *suppress_handler = TRUE;

      g_ptr_array_add (requests, request);
    }

  return requests;
}

static void
connection_new_channel_cb (TpChannelFactoryIface *factory,
                           GObject *chan,
                           gpointer data)
{
  TpBaseConnection *conn = TP_BASE_CONNECTION (data);
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (conn);
  gchar *object_path = NULL, *channel_type = NULL;
  guint handle_type = 0, handle = 0;
  gboolean suppress_handler = priv->suppress_next_handler;
  GPtrArray *tmp;
  guint i;

  g_object_get (chan,
      "object-path", &object_path,
      "channel-type", &channel_type,
      "handle-type", &handle_type,
      "handle", &handle,
      NULL);

  DEBUG ("called for %s", object_path);

  tmp = find_matching_channel_requests (conn, channel_type, handle_type,
                                        handle, &suppress_handler);

  g_signal_emit (conn, signals[NEW_CHANNEL], 0,
                 object_path, channel_type,
                 handle_type, handle,
                 suppress_handler);

  for (i = 0; i < tmp->len; i++)
    {
      ChannelRequest *request = g_ptr_array_index (tmp, i);

      DEBUG ("completing queued request, channel_type=%s, handle_type=%u, "
          "handle=%u, suppress_handler=%u", request->channel_type,
          request->handle_type, request->handle, request->suppress_handler);

      dbus_g_method_return (request->context, object_path);
      request->context = NULL;

      g_ptr_array_remove (priv->channel_requests, request);

      channel_request_free (request);
    }

  g_ptr_array_free (tmp, TRUE);

  g_free (object_path);
  g_free (channel_type);
}

static void
connection_channel_error_cb (TpChannelFactoryIface *factory,
                             GObject *chan,
                             GError *error,
                             gpointer data)
{
  TpBaseConnection *conn = TP_BASE_CONNECTION (data);
  TpBaseConnectionPrivate *priv = TP_BASE_CONNECTION_GET_PRIVATE (conn);
  gchar *channel_type = NULL;
  guint handle_type = 0, handle = 0;
  GPtrArray *tmp;
  guint i;

  DEBUG ("channel_type=%s, handle_type=%u, handle=%u, error_code=%u, "
      "error_message=\"%s\"", channel_type, handle_type, handle,
      error->code, error->message);

  g_object_get (chan,
      "channel-type", &channel_type,
      "handle-type", &handle_type,
      "handle", &handle,
      NULL);

  tmp = find_matching_channel_requests (conn, channel_type, handle_type,
                                        handle, NULL);

  for (i = 0; i < tmp->len; i++)
    {
      ChannelRequest *request = g_ptr_array_index (tmp, i);

      DEBUG ("completing queued request %p, channel_type=%s, "
          "handle_type=%u, handle=%u, suppress_handler=%u",
          request, request->channel_type,
          request->handle_type, request->handle, request->suppress_handler);

      dbus_g_method_return_error (request->context, error);
      request->context = NULL;

      g_ptr_array_remove (priv->channel_requests, request);

      channel_request_free (request);
    }

  g_ptr_array_free (tmp, TRUE);
  g_free (channel_type);
}

static void
tp_base_connection_init (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);
  TpBaseConnectionClass *cls = TP_BASE_CONNECTION_GET_CLASS (self);
  guint i;

  self->priv = priv;

  self->status = TP_INTERNAL_CONNECTION_STATUS_NEW;

  for (i = 0; i <= LAST_TP_HANDLE_TYPE; i++)
    {
      self->handles[i] = NULL;
    }
  (cls->init_handle_repos) (self->handles);

  /* initialize properties mixin */
  tp_properties_mixin_init (G_OBJECT (self), G_STRUCT_OFFSET (
        TpBaseConnection, properties));

  priv->channel_requests = g_ptr_array_new();

  g_assert (cls->create_channel_factories);

  priv->channel_factories = cls->create_channel_factories (self);

  for (i = 0; i < priv->channel_factories->len; i++)
    {
      GObject *factory = g_ptr_array_index (priv->channel_factories, i);
      g_signal_connect (factory, "new-channel", G_CALLBACK
          (connection_new_channel_cb), self);
      g_signal_connect (factory, "channel-error", G_CALLBACK
          (connection_channel_error_cb), self);
    }
}

/**
 * tp_base_connection_register
 *
 * Make the connection object appear on the bus, returning the bus
 * name and object path used.
 */
gboolean
tp_base_connection_register (TpBaseConnection *self,
    gchar **bus_name,
    gchar **object_path,
    GError **error)
{
  TpBaseConnectionClass *cls = TP_BASE_CONNECTION_GET_CLASS (self);
  DBusGConnection *bus;
  DBusGProxy *bus_proxy;
  gchar *tmp;
  gchar *safe_proto;
  gchar *unique_name;
  guint request_name_result;
  GError *request_error;

  g_return_val_if_fail (cls->get_protocol, FALSE);
  g_return_val_if_fail (cls->get_unique_connection_name, FALSE);
  g_return_val_if_fail (cls->cm_dbus_name, FALSE);

  tmp = cls->get_protocol (self);
  safe_proto = tp_escape_as_identifier (tmp);
  g_free (tmp);

  tmp = cls->get_unique_connection_name (self);
  unique_name = tp_escape_as_identifier (tmp);
  g_free (tmp);

  bus = tp_get_bus ();
  bus_proxy = tp_get_bus_proxy ();

  self->bus_name = g_strdup_printf (BUS_NAME_BASE ".%s.%s.%s",
      cls->cm_dbus_name, safe_proto, unique_name);
  self->object_path = g_strdup_printf (OBJECT_PATH_BASE "/%s/%s/%s",
      cls->cm_dbus_name, safe_proto, unique_name);

  if (!dbus_g_proxy_call (bus_proxy, "RequestName", &request_error,
                          G_TYPE_STRING, self->bus_name,
                          G_TYPE_UINT, DBUS_NAME_FLAG_DO_NOT_QUEUE,
                          G_TYPE_INVALID,
                          G_TYPE_UINT, &request_name_result,
                          G_TYPE_INVALID))
    {
      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Error acquiring bus name %s: %s", self->bus_name,
          request_error->message);

      g_error_free (request_error);

      g_free (self->bus_name);
      self->bus_name = NULL;

      return FALSE;
    }

  if (request_name_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    {
      gchar *msg;

      switch (request_name_result)
        {
        case DBUS_REQUEST_NAME_REPLY_IN_QUEUE:
          msg = "Request has been queued, though we request non-queueing.";
          break;
        case DBUS_REQUEST_NAME_REPLY_EXISTS:
          msg = "A connection manger already has this busname.";
          break;
        case DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER:
          msg = "Connection manager already has a connection to this account.";
          break;
        default:
          msg = "Unknown error return from ReleaseName";
        }

      g_set_error (error, TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
          "Error acquiring bus name %s: %s", self->bus_name, msg);

      g_free (self->bus_name);
      self->bus_name = NULL;

      return FALSE;
    }

  DEBUG ("bus name %s", self->bus_name);

  dbus_g_connection_register_g_object (bus, self->object_path, G_OBJECT (self));

  DEBUG ("object path %s", self->object_path);

  *bus_name = g_strdup (self->bus_name);
  *object_path = g_strdup (self->object_path);

  return TRUE;
}

void
tp_base_connection_close_all_channels (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);

  /* trigger close_all on all channel factories */
  g_ptr_array_foreach (priv->channel_factories, (GFunc)
      tp_channel_factory_iface_close_all, NULL);

  /* cancel all queued channel requests */
  if (priv->channel_requests->len > 0)
    {
      g_ptr_array_foreach (priv->channel_requests, (GFunc)
        channel_request_cancel, NULL);
      g_ptr_array_remove_range (priv->channel_requests, 0,
        priv->channel_requests->len);
    }
}

void
tp_base_connection_connecting (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);
  g_ptr_array_foreach (priv->channel_factories, (GFunc)
      tp_channel_factory_iface_connecting, NULL);
}

void
tp_base_connection_connected (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);
  g_ptr_array_foreach (priv->channel_factories, (GFunc)
      tp_channel_factory_iface_connected, NULL);
}

void
tp_base_connection_disconnected (TpBaseConnection *self)
{
  TpBaseConnectionPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      TP_TYPE_BASE_CONNECTION, TpBaseConnectionPrivate);
  g_ptr_array_foreach (priv->channel_factories, (GFunc)
      tp_channel_factory_iface_disconnected, NULL);
}


/**
 * tp_base_connection_get_status
 *
 * Implements D-Bus method GetStatus
 * on interface org.freedesktop.Telepathy.Connection
 *
 * @error: Used to return a pointer to a GError detailing any error
 *         that occurred, D-Bus will throw the error only if this
 *         function returns FALSE.
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
gboolean
tp_base_connection_get_status (TpBaseConnection *self,
                               guint *ret,
                               GError **error)
{
  g_assert (TP_IS_BASE_CONNECTION (self));

  if (self->status == TP_INTERNAL_CONNECTION_STATUS_NEW)
    {
      *ret = TP_CONNECTION_STATUS_DISCONNECTED;
    }
  else
    {
      *ret = self->status;
    }

  return TRUE;
}


/**
 * tp_base_connection_hold_handles
 *
 * Implements D-Bus method HoldHandles
 * on interface org.freedesktop.Telepathy.Connection
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
void
tp_base_connection_hold_handles (TpBaseConnection *self,
                                 guint handle_type,
                                 const GArray *handles,
                                 DBusGMethodInvocation *context)
{
  TpBaseConnectionPrivate *priv;
  GError *error = NULL;
  gchar *sender;
  guint i;

  g_assert (TP_IS_BASE_CONNECTION (self));

  priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  ERROR_IF_NOT_CONNECTED_ASYNC (self, error, context)

  if (!tp_handles_supported_and_valid (self->handles,
        handle_type, handles, FALSE, &error))
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  sender = dbus_g_method_get_sender (context);
  for (i = 0; i < handles->len; i++)
    {
      TpHandle handle = g_array_index (handles, TpHandle, i);
      if (!tp_handle_client_hold (self->handles[handle_type], sender,
            handle, &error))
        {
          dbus_g_method_return_error (context, error);
          g_error_free (error);
          return;
        }
    }

  dbus_g_method_return (context);
}

/**
 * tp_base_connection_inspect_handles
 *
 * Implements D-Bus method InspectHandles
 * on interface org.freedesktop.Telepathy.Connection
 *
 * Returns: TRUE if successful, FALSE if an error was thrown.
 */
void
tp_base_connection_inspect_handles (TpBaseConnection *self,
                                    guint handle_type,
                                    const GArray *handles,
                                    DBusGMethodInvocation *context)
{
  GError *error = NULL;
  const gchar **ret;
  guint i;

  g_assert (TP_IS_BASE_CONNECTION (self));

  ERROR_IF_NOT_CONNECTED_ASYNC (self, error, context);

  if (!tp_handles_supported_and_valid (self->handles,
        handle_type, handles, FALSE, &error))
    {
      dbus_g_method_return_error (context, error);

      g_error_free (error);

      return;
    }

  ret = g_new (const gchar *, handles->len + 1);

  for (i = 0; i < handles->len; i++)
    {
      TpHandle handle;
      const gchar *tmp;

      handle = g_array_index (handles, TpHandle, i);
      tmp = tp_handle_inspect (self->handles[handle_type], handle);
      g_assert (tmp != NULL);

      ret[i] = tmp;
    }

  ret[i] = NULL;

  dbus_g_method_return (context, ret);

  g_free (ret);
}

/**
 * list_channel_factory_foreach_one:
 * @key: iterated key
 * @value: iterated value
 * @data: data attached to this key/value pair
 *
 * Called by the exported ListChannels function, this should iterate over
 * the handle/channel pairs in a channel factory, and to the GPtrArray in
 * the data pointer, add a GValueArray containing the following:
 *  a D-Bus object path for the channel object on this service
 *  a D-Bus interface name representing the channel type
 *  an integer representing the handle type this channel communicates with, or zero
 *  an integer handle representing the contact, room or list this channel communicates with, or zero
 */
static void
list_channel_factory_foreach_one (TpChannelIface *chan,
                                  gpointer data)
{
  GObject *channel = G_OBJECT (chan);
  GPtrArray *channels = (GPtrArray *) data;
  gchar *path, *type;
  guint handle_type, handle;
  GValue entry = { 0, };

  g_value_init (&entry, TP_CHANNEL_LIST_ENTRY_TYPE);
  g_value_take_boxed (&entry, dbus_g_type_specialized_construct
      (TP_CHANNEL_LIST_ENTRY_TYPE));

  g_object_get (channel,
      "object-path", &path,
      "channel-type", &type,
      "handle-type", &handle_type,
      "handle", &handle,
      NULL);

  dbus_g_type_struct_set (&entry,
      0, path,
      1, type,
      2, handle_type,
      3, handle,
      G_MAXUINT);

  g_ptr_array_add (channels, g_value_get_boxed (&entry));

  g_free (path);
  g_free (type);
}

gboolean
tp_base_connection_list_channels (TpBaseConnection *self,
    GPtrArray **ret,
    GError **error)
{
  TpBaseConnectionPrivate *priv;
  GPtrArray *channels;
  guint i;

  g_assert (TP_IS_BASE_CONNECTION (self));

  priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  ERROR_IF_NOT_CONNECTED (self, error)

  /* I think on average, each factory will have 2 channels :D */
  channels = g_ptr_array_sized_new (priv->channel_factories->len * 2);

  for (i = 0; i < priv->channel_factories->len; i++)
    {
      TpChannelFactoryIface *factory = g_ptr_array_index
        (priv->channel_factories, i);
      tp_channel_factory_iface_foreach (factory,
          list_channel_factory_foreach_one, channels);
    }

  *ret = channels;

  return TRUE;
}


/**
 * tp_base_connection_request_channel
 *
 * Implements D-Bus method RequestChannel
 * on interface org.freedesktop.Telepathy.Connection
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
void
tp_base_connection_request_channel (TpBaseConnection *self,
                                    const gchar *type,
                                    guint handle_type,
                                    guint handle,
                                    gboolean suppress_handler,
                                    DBusGMethodInvocation *context)
{
  TpBaseConnectionPrivate *priv;
  TpChannelFactoryRequestStatus status =
    TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_IMPLEMENTED;
  gchar *object_path = NULL;
  GError *error = NULL;
  guint i;

  g_assert (TP_IS_BASE_CONNECTION (self));

  priv = TP_BASE_CONNECTION_GET_PRIVATE (self);

  ERROR_IF_NOT_CONNECTED_ASYNC (self, error, context);

  for (i = 0; i < priv->channel_factories->len; i++)
    {
      TpChannelFactoryIface *factory = g_ptr_array_index
        (priv->channel_factories, i);
      TpChannelFactoryRequestStatus cur_status;
      TpChannelIface *chan = NULL;
      ChannelRequest *request = NULL;

      priv->suppress_next_handler = suppress_handler;

      cur_status = tp_channel_factory_iface_request (factory, type,
          (TpHandleType) handle_type, handle, &chan, &error);

      priv->suppress_next_handler = FALSE;

      switch (cur_status)
        {
        case TP_CHANNEL_FACTORY_REQUEST_STATUS_DONE:
          g_assert (NULL != chan);
          g_object_get (chan, "object-path", &object_path, NULL);
          goto OUT;
        case TP_CHANNEL_FACTORY_REQUEST_STATUS_QUEUED:
          DEBUG ("queueing request, channel_type=%s, handle_type=%u, "
              "handle=%u, suppress_handler=%u", type, handle_type,
              handle, suppress_handler);
          request = channel_request_new (context, type, handle_type, handle,
              suppress_handler);
          g_ptr_array_add (priv->channel_requests, request);
          return;
        case TP_CHANNEL_FACTORY_REQUEST_STATUS_ERROR:
          /* pass through error */
          goto OUT;
        default:
          /* always return the most specific error */
          if (cur_status > status)
            status = cur_status;
        }
    }

  switch (status)
    {
      case TP_CHANNEL_FACTORY_REQUEST_STATUS_INVALID_HANDLE:
        DEBUG ("invalid handle %u", handle);

        error = g_error_new (TP_ERRORS, TP_ERROR_INVALID_HANDLE,
                             "invalid handle %u", handle);

        break;

      case TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_AVAILABLE:
        DEBUG ("requested channel is unavailable with "
                 "handle type %u", handle_type);

        error = g_error_new (TP_ERRORS, TP_ERROR_NOT_AVAILABLE,
                             "requested channel is not available with "
                             "handle type %u", handle_type);

        break;

      case TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_IMPLEMENTED:
        DEBUG ("unsupported channel type %s", type);

        error = g_error_new (TP_ERRORS, TP_ERROR_NOT_IMPLEMENTED,
                             "unsupported channel type %s", type);

        break;

      default:
        g_assert_not_reached ();
    }

OUT:
  if (NULL != error)
    {
      g_assert (NULL == object_path);
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  g_assert (NULL != object_path);
  dbus_g_method_return (context, object_path);
  g_free (object_path);
}


/**
 * tp_base_connection_release_handles
 *
 * Implements D-Bus method ReleaseHandles
 * on interface org.freedesktop.Telepathy.Connection
 *
 * @context: The D-Bus invocation context to use to return values
 *           or throw an error.
 */
void
tp_base_connection_release_handles (TpBaseConnection *self,
                                    guint handle_type,
                                    const GArray * handles,
                                    DBusGMethodInvocation *context)
{
  char *sender;
  GError *error = NULL;
  guint i;

  g_assert (TP_IS_BASE_CONNECTION (self));

  ERROR_IF_NOT_CONNECTED_ASYNC (self, error, context)

  if (!tp_handles_supported_and_valid (self->handles,
        handle_type, handles, FALSE, &error))
    {
      dbus_g_method_return_error (context, error);
      g_error_free (error);
      return;
    }

  sender = dbus_g_method_get_sender (context);
  for (i = 0; i < handles->len; i++)
    {
      TpHandle handle = g_array_index (handles, TpHandle, i);
      if (!tp_handle_client_release (self->handles[handle_type],
            sender, handle, &error))
        {
          dbus_g_method_return_error (context, error);
          g_error_free (error);
          return;
        }
    }

  dbus_g_method_return (context);
}
