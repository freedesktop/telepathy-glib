/*
 * media-observer - Observe media channels
 *
 * Copyright © 2010 Collabora Ltd. <http://www.collabora.co.uk/>
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <glib.h>

#include <telepathy-glib/telepathy-glib.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/simple-observer.h>

static void
chan_invalidated_cb (TpProxy *proxy,
    guint domain,
    gint code,
    gchar *message,
    gpointer user_data)
{
  TpChannel *channel = TP_CHANNEL (proxy);
  GHashTable *props;

  props = tp_channel_borrow_immutable_properties (channel);

  g_print ("Call with %s terminated\n",
      tp_asv_get_string (props, TP_PROP_CHANNEL_TARGET_ID));

  g_object_unref (channel);
}

static void
observe_channels_cb (TpSimpleObserver *self,
    TpAccount *account,
    TpConnection *connection,
    GList *channels,
    TpChannelDispatchOperation *dispatch_operation,
    GList *requests,
    TpObserveChannelsContext *context,
    gpointer user_data)
{
  GList *l;
  gboolean recovering;

  recovering = tp_observe_channels_context_is_recovering (context);

  for (l = channels; l != NULL; l = g_list_next (l))
    {
      TpChannel *channel = l->data;
      GHashTable *props;
      gboolean requested;

      if (tp_strdiff (tp_channel_get_channel_type (channel),
            TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA))
        continue;

      props = tp_channel_borrow_immutable_properties (channel);
      requested = tp_asv_get_boolean (props, TP_PROP_CHANNEL_REQUESTED, NULL);

      g_print ("Observing %s %s call %s %s\n",
          recovering? "existing": "new",
          requested? "outgoing": "incoming",
          requested? "to": "from",
          tp_asv_get_string (props, TP_PROP_CHANNEL_TARGET_ID));

      g_signal_connect (g_object_ref (channel), "invalidated",
          G_CALLBACK (chan_invalidated_cb), NULL);
    }

  tp_observe_channels_context_accept (context);
}

int
main (int argc,
      char **argv)
{
  GMainLoop *mainloop;
  TpDBusDaemon *bus_daemon;
  GError *error = NULL;
  TpBaseClient *observer;

  g_type_init ();
  tp_debug_set_flags (g_getenv ("EXAMPLE_DEBUG"));

  bus_daemon = tp_dbus_daemon_dup (&error);

  if (bus_daemon == NULL)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return 1;
    }

  observer = tp_simple_observer_new (bus_daemon, FALSE, "ExampleMediaObserver",
      FALSE, observe_channels_cb, NULL, NULL);

  tp_base_client_take_observer_filter (observer, tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_STREAMED_MEDIA,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
          TP_HANDLE_TYPE_CONTACT,
        NULL));

  if (!tp_base_client_register (observer, &error))
    {
      g_warning ("Failed to register Observer: %s\n", error->message);
      g_error_free (error);
      goto out;
    }

  g_print ("Start observing\n");

  mainloop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (mainloop);

  if (mainloop != NULL)
    g_main_loop_unref (mainloop);

out:
  g_object_unref (bus_daemon);
  g_object_unref (observer);

  return 0;
}