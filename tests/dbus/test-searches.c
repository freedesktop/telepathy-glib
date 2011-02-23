#include "telepathy-logger/log-manager.c"

#include "lib/util.h"
#include "lib/simple-account.h"
#include "lib/simple-account-manager.h"

#include <telepathy-logger/debug-internal.h>
#include <telepathy-logger/log-manager-internal.h>
#include <telepathy-logger/log-store-internal.h>

#include <telepathy-glib/debug-sender.h>

/* it was defined in telepathy-logger/log-store-pidgin.c */
#undef DEBUG_FLAG
#define DEBUG_FLAG TPL_DEBUG_TESTSUITE
#include <telepathy-logger/debug-internal.h>

#define ACCOUNT_PATH_JABBER TP_ACCOUNT_OBJECT_PATH_BASE "gabble/jabber/user_40collabora_2eco_2euk"
#define MY_ID "user@collabora.co.uk"
#define ID "user2@collabora.co.uk"

typedef struct
{
  GMainLoop *main_loop;

  TpDBusDaemon *dbus;
  TpAccount *account;
  TpTestsSimpleAccount *account_service;

  TplLogManager *manager;
} TestCaseFixture;



#ifdef ENABLE_DEBUG
static TpDebugSender *debug_sender = NULL;
static gboolean stamp_logs = FALSE;


static void
log_to_debug_sender (const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *string)
{
  GTimeVal now;

  g_return_if_fail (TP_IS_DEBUG_SENDER (debug_sender));

  g_get_current_time (&now);

  tp_debug_sender_add_message (debug_sender, &now, log_domain, log_level,
      string);
}


static void
log_handler (const gchar *log_domain,
    GLogLevelFlags log_level,
    const gchar *message,
    gpointer user_data)
{
  if (stamp_logs)
    {
      GTimeVal now;
      gchar now_str[32];
      gchar *tmp;
      struct tm tm;

      g_get_current_time (&now);
      localtime_r (&(now.tv_sec), &tm);
      strftime (now_str, 32, "%Y-%m-%d %H:%M:%S", &tm);
      tmp = g_strdup_printf ("%s.%06ld: %s",
          now_str, now.tv_usec, message);

      g_log_default_handler (log_domain, log_level, tmp, NULL);

      g_free (tmp);
    }
  else
    {
      g_log_default_handler (log_domain, log_level, message, NULL);
    }

  log_to_debug_sender (log_domain, log_level, message);
}
#endif /* ENABLE_DEBUG */



static void
test_get_dates (TestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *ret, *loc;
  TplEntity *entity;

  entity = tpl_entity_new (ID, TPL_ENTITY_CONTACT, NULL, NULL);
  ret = _tpl_log_manager_get_dates (fixture->manager, fixture->account, entity,
      TPL_EVENT_MASK_ANY);
  g_object_unref (entity);

  /* it includes 1 date from libpurple logs, 5 from TpLogger. Empathy
   * log-store date are the same of the TpLogger store, and wont' be present,
   * being duplicates */
  g_assert_cmpint (g_list_length (ret), ==, 6);

  /* we do not want duplicates */
  ret = g_list_sort (ret, (GCompareFunc) g_strcmp0);
  for (loc = ret; loc != NULL; loc = g_list_next (loc))
    if (loc->next)
      g_assert (g_date_compare (loc->data, loc->next->data) != 0);

  g_list_foreach (ret, (GFunc) g_free, NULL);
  g_list_free (ret);
}

static void
test_get_entities (TestCaseFixture *fixture,
    gconstpointer user_data)
{
  GList *ret, *loc;

  ret = _tpl_log_manager_get_entities (fixture->manager, fixture->account);

  g_assert_cmpint (g_list_length (ret), ==, 2);

  /* we do not want duplicates */
  ret = g_list_sort (ret, (GCompareFunc) _tpl_entity_compare);
  for (loc = ret; loc != NULL; loc = g_list_next (loc))
    if (loc->next)
      g_assert (_tpl_entity_compare (loc->data, loc->next->data) != 0);

  g_list_foreach (ret, (GFunc) g_object_unref, NULL);
  g_list_free (ret);
}

static void
teardown_service (TestCaseFixture* fixture,
    gconstpointer user_data)
{
  GError *error = NULL;

  g_assert (user_data != NULL);

  if (fixture->account != NULL)
    {
      /* FIXME is it useful in this suite */
      tp_tests_proxy_run_until_dbus_queue_processed (fixture->account);

      g_object_unref (fixture->account);
      fixture->account = NULL;
    }

  tp_dbus_daemon_unregister_object (fixture->dbus, fixture->account_service);
  g_object_unref (fixture->account_service);
  fixture->account_service = NULL;

  tp_dbus_daemon_release_name (fixture->dbus, TP_ACCOUNT_MANAGER_BUS_NAME,
      &error);
  g_assert_no_error (error);

  g_object_unref (fixture->dbus);
  fixture->dbus = NULL;
}

static void
teardown (TestCaseFixture* fixture,
    gconstpointer user_data)
{
  g_object_unref (fixture->manager);
  fixture->manager = NULL;

  if (user_data != NULL)
    teardown_service (fixture, user_data);

  g_main_loop_unref (fixture->main_loop);
  fixture->main_loop = NULL;
}


static void
account_prepare_cb (GObject *source,
    GAsyncResult *result,
    gpointer user_data)
{
  TestCaseFixture *fixture = user_data;
  GError *error = NULL;

  tp_proxy_prepare_finish (source, result, &error);
  g_assert_no_error (error);

  g_main_loop_quit (fixture->main_loop);
}


static void
setup_service (TestCaseFixture* fixture,
    gconstpointer user_data)
{
  GQuark account_features[] = { TP_ACCOUNT_FEATURE_CORE, 0 };
  const gchar *account_path;
  GValue *boxed_params;
  GHashTable *params = (GHashTable *) user_data;
  GError *error = NULL;

  g_assert (params != NULL);

  fixture->dbus = tp_tests_dbus_daemon_dup_or_die ();
  g_assert (fixture->dbus != NULL);

  tp_dbus_daemon_request_name (fixture->dbus,
      TP_ACCOUNT_MANAGER_BUS_NAME, FALSE, &error);
  g_assert_no_error (error);

  /* Create service-side Account object with the passed parameters */
  fixture->account_service = g_object_new (TP_TESTS_TYPE_SIMPLE_ACCOUNT,
      NULL);
  g_assert (fixture->account_service != NULL);

  /* account-path will be set-up as parameter as well, this is not an issue */
  account_path = g_value_get_string (
      (const GValue *) g_hash_table_lookup (params, "account-path"));
  g_assert (account_path != NULL);

  boxed_params = tp_g_value_slice_new_boxed (TP_HASH_TYPE_STRING_VARIANT_MAP,
      params);
  g_object_set_property (G_OBJECT (fixture->account_service),
      "parameters", boxed_params);

  tp_dbus_daemon_register_object (fixture->dbus, account_path,
      fixture->account_service);

  fixture->account = tp_account_new (fixture->dbus, account_path, NULL);
  g_assert (fixture->account != NULL);
  tp_proxy_prepare_async (fixture->account, account_features,
      account_prepare_cb, fixture);
  g_main_loop_run (fixture->main_loop);

  g_assert (tp_account_is_prepared (fixture->account,
        TP_ACCOUNT_FEATURE_CORE));

  tp_g_value_slice_free (boxed_params);
}

static void
setup (TestCaseFixture* fixture,
    gconstpointer user_data)
{
  DEBUG ("setting up");

  fixture->main_loop = g_main_loop_new (NULL, FALSE);
  g_assert (fixture->main_loop != NULL);

  fixture->manager = tpl_log_manager_dup_singleton ();

  if (user_data != NULL)
    setup_service (fixture, user_data);

  DEBUG ("set up finished");
}

static void
setup_debug (void)
{
  tp_debug_divert_messages (g_getenv ("TPL_LOGFILE"));

#ifdef ENABLE_DEBUG
  _tpl_debug_set_flags_from_env ();

  stamp_logs = (g_getenv ("TPL_TIMING") != NULL);
  debug_sender = tp_debug_sender_dup ();

  g_log_set_default_handler (log_handler, NULL);
#endif /* ENABLE_DEBUG */
}


int
main (int argc, char **argv)
{
  GHashTable *params = NULL;
  GList *l = NULL;
  int retval;

  g_type_init ();

  setup_debug ();

  /* no account tests */
  g_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugs.freedesktop.org/show_bug.cgi?id=");

  /* account related tests */
  params = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      (GDestroyNotify) tp_g_value_slice_free);
  g_assert (params != NULL);

  l = g_list_prepend (l, params);

  g_hash_table_insert (params, "account",
      tp_g_value_slice_new_static_string (MY_ID));
  g_hash_table_insert (params, "account-path",
      tp_g_value_slice_new_static_string (ACCOUNT_PATH_JABBER));

  g_test_add ("/log-manager/get-dates",
      TestCaseFixture, params,
      setup, test_get_dates, teardown);

  g_test_add ("/log-manager/get-entities",
      TestCaseFixture, params,
      setup, test_get_entities, teardown);


  retval = g_test_run ();

  g_list_foreach (l, (GFunc) g_hash_table_unref, NULL);

  return retval;
}