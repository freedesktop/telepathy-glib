/* Basic introspection on a channel (template for further regression tests)
 *
 * Copyright (C) 2007-2008 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2007-2008 Nokia Corporation
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty provided the copyright
 * notice and this notice are preserved.
 */

#include <telepathy-glib/channel.h>
#include <telepathy-glib/connection.h>
#include <telepathy-glib/dbus.h>
#include <telepathy-glib/debug.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/proxy-subclass.h>

#include "tests/lib/myassert.h"
#include "tests/lib/simple-conn.h"
#include "tests/lib/textchan-null.h"
#include "tests/lib/util.h"

#define IDENTIFIER "them@example.org"

static GError *invalidated = NULL;
static GMainLoop *mainloop;

static void
channel_ready (TpChannel *channel,
               const GError *error,
               gpointer user_data)
{
  gboolean *set = user_data;

  *set = TRUE;

  if (error == NULL)
    {
      g_message ("channel %p ready", channel);
    }
  else
    {
      g_message ("channel %p invalidated: %s #%u \"%s\"", channel,
          g_quark_to_string (error->domain), error->code, error->message);

      invalidated = g_error_copy (error);
    }

  if (mainloop != NULL)
    g_main_loop_quit (mainloop);
}

static void
channel_prepared_cb (GObject *object,
    GAsyncResult *res,
    gpointer user_data)
{
  GAsyncResult **output = user_data;

  g_message ("channel %p prepared", object);
  *output = g_object_ref (res);

  if (mainloop != NULL)
    g_main_loop_quit (mainloop);
}

static void
assert_chan_sane (TpChannel *chan,
                  TpHandle handle)
{
  GHashTable *asv;
  TpHandleType type;

  MYASSERT (tp_channel_is_ready (chan), "");
  MYASSERT (tp_channel_get_handle (chan, NULL) == handle, "");
  MYASSERT (tp_channel_get_handle (chan, &type) == handle, "");
  MYASSERT (type == TP_HANDLE_TYPE_CONTACT, "%u", type);
  MYASSERT (g_str_equal (tp_channel_get_channel_type (chan),
        TP_IFACE_CHANNEL_TYPE_TEXT), "");
  MYASSERT (tp_channel_get_channel_type_id (chan) ==
        TP_IFACE_QUARK_CHANNEL_TYPE_TEXT, "");
  MYASSERT (TP_IS_CONNECTION (tp_channel_borrow_connection (chan)), "");
  g_assert_cmpstr (tp_channel_get_identifier (chan), ==, IDENTIFIER);

  asv = tp_channel_borrow_immutable_properties (chan);
  MYASSERT (asv != NULL, "");
  g_assert_cmpstr (
      tp_asv_get_string (asv, TP_PROP_CHANNEL_CHANNEL_TYPE), ==,
      TP_IFACE_CHANNEL_TYPE_TEXT);
  g_assert_cmpuint (
      tp_asv_get_uint32 (asv, TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, NULL), ==,
      TP_HANDLE_TYPE_CONTACT);
  g_assert_cmpuint (
      tp_asv_get_uint32 (asv, TP_PROP_CHANNEL_TARGET_HANDLE, NULL), ==,
      handle);
  g_assert_cmpstr (
      tp_asv_get_string (asv, TP_PROP_CHANNEL_TARGET_ID), ==,
      IDENTIFIER);
}

int
main (int argc,
      char **argv)
{
  TpTestsSimpleConnection *service_conn;
  TpBaseConnection *service_conn_as_base;
  TpHandleRepoIface *contact_repo;
  TestTextChannelNull *service_chan;
  TestPropsTextChannel *service_props_chan;
  TestPropsGroupTextChannel *service_props_group_chan;
  TpDBusDaemon *dbus;
  TpConnection *conn;
  TpChannel *chan;
  GError *error = NULL;
  gchar *name;
  gchar *conn_path;
  gchar *chan_path;
  gchar *props_chan_path;
  gchar *props_group_chan_path;
  gchar *bad_chan_path;
  TpHandle handle;
  gboolean was_ready;
  GError invalidated_for_test = { TP_ERRORS, TP_ERROR_PERMISSION_DENIED,
      "No channel for you!" };
  GHashTable *asv;
  GAsyncResult *prepare_result;
  GQuark some_features[] = { TP_CHANNEL_FEATURE_CORE,
      TP_CHANNEL_FEATURE_CHAT_STATES, 0 };

  g_type_init ();
  tp_debug_set_flags ("all");
  dbus = tp_tests_dbus_daemon_dup_or_die ();

  service_conn = TP_TESTS_SIMPLE_CONNECTION (tp_tests_object_new_static_class (
        TP_TESTS_TYPE_SIMPLE_CONNECTION,
        "account", "me@example.com",
        "protocol", "simple",
        NULL));
  service_conn_as_base = TP_BASE_CONNECTION (service_conn);
  MYASSERT (service_conn != NULL, "");
  MYASSERT (service_conn_as_base != NULL, "");

  MYASSERT (tp_base_connection_register (service_conn_as_base, "simple",
        &name, &conn_path, &error), "");
  g_assert_no_error (error);

  conn = tp_connection_new (dbus, name, conn_path, &error);
  MYASSERT (conn != NULL, "");
  g_assert_no_error (error);

  MYASSERT (tp_connection_run_until_ready (conn, TRUE, &error, NULL),
      "");
  g_assert_no_error (error);

  contact_repo = tp_base_connection_get_handles (service_conn_as_base,
      TP_HANDLE_TYPE_CONTACT);
  MYASSERT (contact_repo != NULL, "");

  handle = tp_handle_ensure (contact_repo, IDENTIFIER, NULL, &error);
  g_assert_no_error (error);

  chan_path = g_strdup_printf ("%s/Channel", conn_path);

  service_chan = TEST_TEXT_CHANNEL_NULL (tp_tests_object_new_static_class (
        TEST_TYPE_TEXT_CHANNEL_NULL,
        "connection", service_conn,
        "object-path", chan_path,
        "handle", handle,
        NULL));

  props_chan_path = g_strdup_printf ("%s/PropertiesChannel", conn_path);

  service_props_chan = TEST_PROPS_TEXT_CHANNEL (
      tp_tests_object_new_static_class (
        TEST_TYPE_PROPS_TEXT_CHANNEL,
        "connection", service_conn,
        "object-path", props_chan_path,
        "handle", handle,
        NULL));

  props_group_chan_path = g_strdup_printf ("%s/PropsGroupChannel", conn_path);

  service_props_group_chan = TEST_PROPS_GROUP_TEXT_CHANNEL (
      tp_tests_object_new_static_class (
        TEST_TYPE_PROPS_GROUP_TEXT_CHANNEL,
        "connection", service_conn,
        "object-path", props_group_chan_path,
        "handle", handle,
        NULL));

  mainloop = g_main_loop_new (NULL, FALSE);

  g_message ("Channel becomes invalid while we wait");

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      FALSE);
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CHAT_STATES),
      ==, FALSE);

  tp_proxy_invalidate ((TpProxy *) chan, &invalidated_for_test);

  prepare_result = NULL;
  tp_proxy_prepare_async (chan, NULL, channel_prepared_cb, &prepare_result);

  MYASSERT (!tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT (error != NULL, "");
  g_assert_error (error, invalidated_for_test.domain,
      invalidated_for_test.code);
  g_assert_cmpstr (error->message, ==, invalidated_for_test.message);
  g_error_free (error);
  error = NULL;

  if (prepare_result == NULL)
    g_main_loop_run (mainloop);

  MYASSERT (!tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_error (error, invalidated_for_test.domain,
      invalidated_for_test.code);
  g_assert_cmpstr (error->message, ==, invalidated_for_test.message);
  g_clear_error (&error);
  /* it was never ready */
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      FALSE);
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CHAT_STATES),
      ==, FALSE);

  g_object_unref (prepare_result);
  prepare_result = NULL;

  g_object_unref (chan);
  chan = NULL;

  g_message ("Channel becomes invalid and we are called back synchronously");

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  was_ready = FALSE;
  tp_proxy_prepare_async (chan, NULL, channel_prepared_cb, &prepare_result);

  /* no way to see what this is doing - just make sure it doesn't crash */
  tp_proxy_prepare_async (chan, some_features, NULL, NULL);

  tp_channel_call_when_ready (chan, channel_ready, &was_ready);
  tp_proxy_invalidate ((TpProxy *) chan, &invalidated_for_test);
  MYASSERT (was_ready == TRUE, "");
  MYASSERT (invalidated != NULL, "");
  g_assert_error (invalidated, invalidated_for_test.domain,
      invalidated_for_test.code);
  g_assert_cmpstr (invalidated->message, ==, invalidated_for_test.message);
  g_error_free (invalidated);
  invalidated = NULL;

  /* prepare_async never calls back synchronously */
  MYASSERT (prepare_result == NULL, "");
  g_main_loop_run (mainloop);
  MYASSERT (!tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_error (error, invalidated_for_test.domain,
      invalidated_for_test.code);
  g_assert_cmpstr (error->message, ==, invalidated_for_test.message);
  g_clear_error (&error);
  g_object_unref (prepare_result);
  prepare_result = NULL;
  /* it was never ready */
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      FALSE);
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CHAT_STATES),
      ==, FALSE);

  g_object_unref (chan);
  chan = NULL;

  g_message ("Channel becomes ready while we wait");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  service_chan->get_handle_called = 0;
  service_chan->get_interfaces_called = 0;
  service_chan->get_channel_type_called = 0;

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  prepare_result = NULL;
  tp_proxy_prepare_async (chan, NULL, channel_prepared_cb, &prepare_result);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  g_assert_no_error (error);
  g_assert_cmpuint (service_chan->get_handle_called, ==, 0);
  g_assert_cmpuint (service_chan->get_interfaces_called, ==, 1);
  g_assert_cmpuint (service_chan->get_channel_type_called, ==, 0);

  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      TRUE);
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CHAT_STATES),
      ==, FALSE);

  if (prepare_result == NULL)
    g_main_loop_run (mainloop);

  MYASSERT (tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_no_error (error);

  g_object_unref (prepare_result);
  prepare_result = NULL;

  assert_chan_sane (chan, handle);

  g_object_unref (chan);
  chan = NULL;

  g_message ("Channel becomes ready while we wait (the version with "
      "Properties)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_handle_called = 0;
  TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_interfaces_called = 0;
  TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_channel_type_called = 0;

  chan = tp_channel_new (conn, props_chan_path, NULL,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);
  g_assert_no_error (error);

  prepare_result = NULL;
  tp_proxy_prepare_async (chan, some_features, channel_prepared_cb,
      &prepare_result);

  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      FALSE);
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CHAT_STATES),
      ==, FALSE);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  g_assert_no_error (error);
  g_assert_cmpuint (
      TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_handle_called, ==, 0);
  g_assert_cmpuint (
      TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_channel_type_called,
      ==, 0);
  g_assert_cmpuint (
      TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_interfaces_called, ==,
      0);

  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      TRUE);
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CHAT_STATES),
      ==, FALSE);

  if (prepare_result == NULL)
    g_main_loop_run (mainloop);

  MYASSERT (tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_no_error (error);

  g_object_unref (prepare_result);
  prepare_result = NULL;

  assert_chan_sane (chan, handle);

  /* no way to see what this is doing - just make sure it doesn't crash */
  tp_proxy_prepare_async (chan, some_features, NULL, NULL);

  prepare_result = NULL;
  tp_proxy_prepare_async (chan, some_features, channel_prepared_cb,
      &prepare_result);

  if (prepare_result == NULL)
    g_main_loop_run (mainloop);

  MYASSERT (tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_no_error (error);

  g_object_unref (prepare_result);
  prepare_result = NULL;

  g_object_unref (chan);
  chan = NULL;

  g_message ("Channel becomes ready while we wait (preloading immutable "
      "properties)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_handle_called = 0;
  TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_interfaces_called = 0;
  TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_channel_type_called = 0;

  g_hash_table_remove_all (TEST_PROPS_TEXT_CHANNEL (service_props_chan)
      ->dbus_property_interfaces_retrieved);

  asv = tp_asv_new (
      TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
          TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT, TP_HANDLE_TYPE_CONTACT,
      TP_PROP_CHANNEL_TARGET_HANDLE, G_TYPE_UINT, handle,
      TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, IDENTIFIER,
      TP_PROP_CHANNEL_INITIATOR_HANDLE, G_TYPE_UINT, handle,
      TP_PROP_CHANNEL_INITIATOR_ID, G_TYPE_STRING, IDENTIFIER,
      TP_PROP_CHANNEL_INTERFACES, G_TYPE_STRV, NULL,
      TP_PROP_CHANNEL_REQUESTED, G_TYPE_BOOLEAN, FALSE,
      NULL);

  chan = tp_channel_new_from_properties (conn, props_chan_path, asv, &error);
  g_assert_no_error (error);

  g_hash_table_destroy (asv);
  asv = NULL;

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  g_assert_no_error (error);
  g_assert_cmpuint (g_hash_table_size (
      service_props_chan->dbus_property_interfaces_retrieved), ==, 0);
  g_assert_cmpuint (
      TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_handle_called, ==, 0);
  g_assert_cmpuint (
      TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_channel_type_called,
      ==, 0);
  /* FIXME: with an improved fast-path we could avoid this one too maybe? */
  /*
  g_assert_cmpuint (
      TEST_TEXT_CHANNEL_NULL (service_props_chan)->get_interfaces_called,
      ==, 0);
   */

  assert_chan_sane (chan, handle);

  g_object_unref (chan);
  chan = NULL;

  g_message ("Group channel becomes ready while we wait (preloading immutable "
      "properties)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  TEST_TEXT_CHANNEL_NULL (service_props_group_chan)->get_handle_called = 0;
  TEST_TEXT_CHANNEL_NULL (service_props_group_chan)->get_interfaces_called = 0;
  TEST_TEXT_CHANNEL_NULL (service_props_group_chan)->get_channel_type_called
      = 0;

  g_hash_table_remove_all (TEST_PROPS_TEXT_CHANNEL (service_props_group_chan)
      ->dbus_property_interfaces_retrieved);

  {
    const gchar *interfaces[] = {
        TP_IFACE_CHANNEL_INTERFACE_GROUP,
        NULL
    };

    asv = tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
            TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
            TP_HANDLE_TYPE_CONTACT,
        TP_PROP_CHANNEL_TARGET_HANDLE, G_TYPE_UINT, handle,
        TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, IDENTIFIER,
        TP_PROP_CHANNEL_INITIATOR_HANDLE, G_TYPE_UINT, handle,
        TP_PROP_CHANNEL_INITIATOR_ID, G_TYPE_STRING, IDENTIFIER,
        TP_PROP_CHANNEL_INTERFACES, G_TYPE_STRV, interfaces,
        TP_PROP_CHANNEL_REQUESTED, G_TYPE_BOOLEAN, FALSE,
        NULL);
  }

  chan = tp_channel_new_from_properties (conn, props_group_chan_path, asv, &error);
  g_assert_no_error (error);

  g_hash_table_destroy (asv);
  asv = NULL;

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  g_assert_no_error (error);
  g_assert_cmpuint (TEST_TEXT_CHANNEL_NULL (service_props_group_chan)
      ->get_handle_called, ==, 0);
  g_assert_cmpuint (TEST_TEXT_CHANNEL_NULL (service_props_group_chan)
      ->get_channel_type_called, ==, 0);
  g_assert_cmpuint (TEST_TEXT_CHANNEL_NULL (service_props_group_chan)
      ->get_interfaces_called, ==, 0);
  g_assert_cmpuint (g_hash_table_size (
      TEST_PROPS_TEXT_CHANNEL (service_props_group_chan)
      ->dbus_property_interfaces_retrieved), ==, 1);
  MYASSERT (g_hash_table_lookup (
      TEST_PROPS_TEXT_CHANNEL (service_props_group_chan)
      ->dbus_property_interfaces_retrieved,
      GUINT_TO_POINTER (TP_IFACE_QUARK_CHANNEL_INTERFACE_GROUP)) != NULL,
      "Only Chan.I.Group's properties should have been retrieved");

  assert_chan_sane (chan, handle);

  g_object_unref (chan);
  chan = NULL;

  g_message ("Channel becomes ready while we wait (in the case where we "
      "have to discover the channel type)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  service_chan->get_handle_called = 0;
  service_chan->get_interfaces_called = 0;
  service_chan->get_channel_type_called = 0;

  chan = tp_channel_new (conn, chan_path, NULL,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  g_assert_no_error (error);
  g_assert_cmpuint (service_chan->get_handle_called, ==, 0);
  g_assert_cmpuint (service_chan->get_interfaces_called, ==, 1);
  g_assert_cmpuint (service_chan->get_channel_type_called, ==, 1);

  assert_chan_sane (chan, handle);

  g_object_unref (chan);
  chan = NULL;

  g_message ("Channel becomes ready while we wait (in the case where we "
      "have to discover the handle type)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  service_chan->get_handle_called = 0;
  service_chan->get_interfaces_called = 0;
  service_chan->get_channel_type_called = 0;

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);
  g_assert_no_error (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  g_assert_no_error (error);
  g_assert_cmpuint (service_chan->get_handle_called, ==, 1);
  g_assert_cmpuint (service_chan->get_interfaces_called, ==, 1);
  g_assert_cmpuint (service_chan->get_channel_type_called, ==, 0);

  assert_chan_sane (chan, handle);

  g_object_unref (chan);
  chan = NULL;

  g_message ("Channel becomes ready while we wait (in the case where we "
      "have to discover the handle)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  service_chan->get_handle_called = 0;
  service_chan->get_interfaces_called = 0;
  service_chan->get_channel_type_called = 0;

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, 0, &error);
  g_assert_no_error (error);

  MYASSERT (tp_channel_run_until_ready (chan, &error, NULL), "");
  g_assert_no_error (error);
  g_assert_cmpuint (service_chan->get_handle_called, ==, 1);
  g_assert_cmpuint (service_chan->get_interfaces_called, ==, 1);
  g_assert_cmpuint (service_chan->get_channel_type_called, ==, 0);

  assert_chan_sane (chan, handle);

  g_object_unref (chan);
  chan = NULL;

  g_message ("channel does not, in fact, exist (callback)");

  bad_chan_path = g_strdup_printf ("%s/Does/Not/Actually/Exist", conn_path);
  chan = tp_channel_new (conn, bad_chan_path, NULL,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);
  g_assert_no_error (error);

  was_ready = FALSE;
  tp_channel_call_when_ready (chan, channel_ready, &was_ready);
  g_main_loop_run (mainloop);
  MYASSERT (was_ready == TRUE, "");
  MYASSERT (invalidated != NULL, "");
  MYASSERT (invalidated->domain == DBUS_GERROR,
      "%s", g_quark_to_string (invalidated->domain));
  MYASSERT (invalidated->code == DBUS_GERROR_UNKNOWN_METHOD,
      "%u", invalidated->code);
  g_error_free (invalidated);
  invalidated = NULL;

  g_object_unref (chan);
  chan = NULL;
  g_free (bad_chan_path);
  bad_chan_path = NULL;

  g_message ("channel does not, in fact, exist (run_until_ready)");

  bad_chan_path = g_strdup_printf ("%s/Does/Not/Actually/Exist", conn_path);
  chan = tp_channel_new (conn, bad_chan_path, NULL,
      TP_UNKNOWN_HANDLE_TYPE, 0, &error);
  g_assert_no_error (error);

  MYASSERT (!tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT (error != NULL, "");
  MYASSERT (error->domain == DBUS_GERROR,
      "%s", g_quark_to_string (error->domain));
  MYASSERT (error->code == DBUS_GERROR_UNKNOWN_METHOD,
      "%u", error->code);
  g_error_free (error);
  error = NULL;

  g_object_unref (chan);
  chan = NULL;
  g_free (bad_chan_path);
  bad_chan_path = NULL;

  g_message ("Channel doesn't actually implement Group (preloading immutable "
      "properties)");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  service_chan->get_handle_called = 0;
  service_chan->get_interfaces_called = 0;
  service_chan->get_channel_type_called = 0;

  {
    const gchar *interfaces[] = {
        TP_IFACE_CHANNEL_INTERFACE_GROUP,
        NULL
    };

    asv = tp_asv_new (
        TP_PROP_CHANNEL_CHANNEL_TYPE, G_TYPE_STRING,
            TP_IFACE_CHANNEL_TYPE_TEXT,
        TP_PROP_CHANNEL_TARGET_HANDLE_TYPE, G_TYPE_UINT,
            TP_HANDLE_TYPE_CONTACT,
        TP_PROP_CHANNEL_TARGET_HANDLE, G_TYPE_UINT, handle,
        TP_PROP_CHANNEL_TARGET_ID, G_TYPE_STRING, IDENTIFIER,
        TP_PROP_CHANNEL_INITIATOR_HANDLE, G_TYPE_UINT, handle,
        TP_PROP_CHANNEL_INITIATOR_ID, G_TYPE_STRING, IDENTIFIER,
        TP_PROP_CHANNEL_INTERFACES, G_TYPE_STRV, interfaces,
        TP_PROP_CHANNEL_REQUESTED, G_TYPE_BOOLEAN, FALSE,
        NULL);
  }

  /* We lie and say that the basic Text channel has the Group interface; this
   * should make introspection fail.
   */
  chan = tp_channel_new_from_properties (conn, chan_path, asv, &error);
  g_assert_no_error (error);

  g_hash_table_destroy (asv);
  asv = NULL;

  MYASSERT (!tp_channel_run_until_ready (chan, &error, NULL), "");
  MYASSERT (error != NULL, "");
  MYASSERT (error->domain == DBUS_GERROR,
      "%s", g_quark_to_string (error->domain));
  MYASSERT (error->code == DBUS_GERROR_UNKNOWN_METHOD,
      "%u", error->code);
  g_error_free (error);
  error = NULL;

  g_assert_cmpuint (service_chan->get_handle_called, ==, 0);
  g_assert_cmpuint (service_chan->get_channel_type_called, ==, 0);
  g_assert_cmpuint (service_chan->get_interfaces_called, ==, 0);

  g_object_unref (chan);
  chan = NULL;

  g_message ("Channel becomes ready and we are called back");

  tp_tests_proxy_run_until_dbus_queue_processed (conn);

  service_chan->get_handle_called = 0;
  service_chan->get_interfaces_called = 0;
  service_chan->get_channel_type_called = 0;

  chan = tp_channel_new (conn, chan_path, TP_IFACE_CHANNEL_TYPE_TEXT,
      TP_HANDLE_TYPE_CONTACT, handle, &error);
  g_assert_no_error (error);

  was_ready = FALSE;
  tp_channel_call_when_ready (chan, channel_ready, &was_ready);
  g_message ("Entering main loop");
  g_main_loop_run (mainloop);
  g_message ("Leaving main loop");
  MYASSERT (was_ready == TRUE, "");
  g_assert_no_error (invalidated);
  g_assert_cmpuint (service_chan->get_handle_called, ==, 0);
  g_assert_cmpuint (service_chan->get_interfaces_called, ==, 1);
  g_assert_cmpuint (service_chan->get_channel_type_called, ==, 0);

  assert_chan_sane (chan, handle);

  /* ... keep the same channel for the next test */

  g_message ("Channel already ready, so we are called back synchronously");

  was_ready = FALSE;
  tp_channel_call_when_ready (chan, channel_ready, &was_ready);
  MYASSERT (was_ready == TRUE, "");
  g_assert_no_error (invalidated);

  assert_chan_sane (chan, handle);

  /* ... keep the same channel for the next test */

  g_message ("Channel already dead, so we are called back synchronously");

  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      TRUE);
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CHAT_STATES),
      ==, FALSE);

  MYASSERT (tp_cli_connection_run_disconnect (conn, -1, &error, NULL), "");
  g_assert_no_error (error);

  was_ready = FALSE;

  prepare_result = NULL;
  tp_proxy_prepare_async (chan, some_features, channel_prepared_cb,
      &prepare_result);

  tp_channel_call_when_ready (chan, channel_ready, &was_ready);
  MYASSERT (was_ready == TRUE, "");
  MYASSERT (invalidated != NULL, "");
  MYASSERT (invalidated->domain == TP_ERRORS,
      ": %s", g_quark_to_string (invalidated->domain));
  MYASSERT (invalidated->code == TP_ERROR_CANCELLED,
      ": %u", invalidated->code);

  /* is_prepared becomes FALSE because the channel broke */
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CORE), ==,
      FALSE);
  g_assert_cmpint (tp_proxy_is_prepared (chan, TP_CHANNEL_FEATURE_CHAT_STATES),
      ==, FALSE);
  g_assert_error (invalidated, tp_proxy_get_invalidated (chan)->domain,
      tp_proxy_get_invalidated (chan)->code);
  g_assert_cmpstr (invalidated->message, ==,
      tp_proxy_get_invalidated (chan)->message);

  /* ... but prepare_async still hasn't finished until we run the main loop */
  g_assert (prepare_result == NULL);
  g_main_loop_run (mainloop);
  g_assert (prepare_result != NULL);
  MYASSERT (!tp_proxy_prepare_finish (chan, prepare_result, &error), "");
  g_assert_error (error, invalidated->domain, invalidated->code);
  g_assert_cmpstr (error->message, ==, invalidated->message);

  g_clear_error (&error);
  g_clear_error (&invalidated);

  g_object_unref (chan);
  chan = NULL;

  /* clean up */

  g_assert (chan == NULL);
  g_main_loop_unref (mainloop);
  mainloop = NULL;

  tp_handle_unref (contact_repo, handle);
  g_object_unref (conn);
  g_object_unref (service_chan);
  g_object_unref (service_props_chan);
  g_object_unref (service_props_group_chan);

  service_conn_as_base = NULL;
  g_object_unref (service_conn);
  g_object_unref (dbus);
  g_free (name);
  g_free (conn_path);
  g_free (chan_path);
  g_free (props_chan_path);
  g_free (props_group_chan_path);

  return 0;
}
