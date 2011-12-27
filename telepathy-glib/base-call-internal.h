/*
 * base-call-internal.h - Header for TpBaseCall* (internals)
 * Copyright © 2011 Collabora Ltd.
 * @author Olivier Crete <olivier.crete@collabora.com>
 * @author Xavier Claessens <xavier.claessens@collabora.co.uk>
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

#ifndef __TP_BASE_CALL_INTERNAL_H__
#define __TP_BASE_CALL_INTERNAL_H__

#include <telepathy-glib/base-connection.h>

G_BEGIN_DECLS

/* Forward declaration */
typedef struct _TpBaseCallChannel TpBaseCallChannel;
typedef struct _TpBaseCallContent TpBaseCallContent;
typedef struct _TpBaseCallStream  TpBaseCallStream;
typedef struct _TpCallContentMediaDescription  TpCallContentMediaDescription;
typedef struct _TpBaseMediaCallContent TpBaseMediaCallContent;
typedef struct _TpBaseMediaCallStream TpBaseMediaCallStream;
typedef struct _TpCallStreamEndpoint TpCallStreamEndpoint;

/* Implemented in base-call-content.c */
void _tp_base_call_content_set_channel (TpBaseCallContent *self,
    TpBaseCallChannel *channel);
TpBaseCallChannel *_tp_base_call_content_get_channel (TpBaseCallContent *self);
void _tp_base_call_content_accepted (TpBaseCallContent *self,
    TpHandle actor_handle);
void _tp_base_call_content_deinit (TpBaseCallContent *self);
void _tp_base_call_content_remove_stream_internal (TpBaseCallContent *self,
    TpBaseCallStream *stream,
    const GValueArray *reason_array);

/* Implemented in base-media-call-content.c */
gboolean _tp_base_media_call_content_ready_to_accept (
    TpBaseMediaCallContent *self);
void _tp_base_media_call_content_remote_accepted (TpBaseMediaCallContent *self);

/* Implemented in base-call-stream.c */
void _tp_base_call_stream_set_content (TpBaseCallStream *self,
    TpBaseCallContent *content);
TpBaseCallContent *_tp_base_call_stream_get_content (TpBaseCallStream *self);
TpBaseCallChannel *_tp_base_call_stream_get_channel (TpBaseCallStream *self);
gboolean _tp_base_call_stream_set_sending (TpBaseCallStream *self,
    gboolean send,
    TpHandle actor_handle,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message,
    GError **error);
GHashTable *_tp_base_call_stream_borrow_remote_members (
    TpBaseCallStream *stream);

/* Implemented in base-call-channel.c */
GHashTable *_tp_base_call_dup_member_identifiers (TpBaseConnection *conn,
    GHashTable *source);
GValueArray *_tp_base_call_state_reason_new (TpHandle actor_handle,
    TpCallStateChangeReason reason,
    const gchar *dbus_reason,
    const gchar *message);
void _tp_base_call_channel_remove_content_internal (TpBaseCallChannel *self,
    TpBaseCallContent *content,
    const GValueArray *reason_array);
void _tp_base_call_channel_maybe_initizalised (TpBaseCallChannel *self);
void _tp_base_call_channel_set_locally_muted (TpBaseCallChannel *self,
    gboolean locally_muted);

/* Implemented in base-media-call-channel.c */
void _tp_base_media_call_stream_start_receiving (
    TpBaseMediaCallStream *self, guint contact);

/* Implemented in call-content-media-description.c */
void _tp_call_content_media_description_offer_async (
    TpCallContentMediaDescription *self,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);
gboolean _tp_call_content_media_description_offer_finish (
    TpCallContentMediaDescription *self,
    GAsyncResult *result,
    GHashTable **properties,
    GError **error);
GHashTable *_tp_call_content_media_description_dup_properties (
    TpCallContentMediaDescription *self);

/* Implemented in call-stream-endpoint.c */
void _tp_call_stream_endpoint_set_stream (TpCallStreamEndpoint *self,
    TpBaseMediaCallStream *stream);

G_END_DECLS

#endif /* #ifndef __TP_BASE_CALL_INTERNAL_H__*/
