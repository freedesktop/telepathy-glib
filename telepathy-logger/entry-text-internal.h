/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009 Collabora Ltd.
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
 *
 * Authors: Cosimo Alfarano <cosimo.alfarano@collabora.co.uk>
 */

#ifndef __TPL_ENTRY_TEXT_INTERNAL_H__
#define __TPL_ENTRY_TEXT_INTERNAL_H__

#include <telepathy-logger/entry-text.h>
#include <telepathy-logger/entry-internal.h>
#include <telepathy-logger/channel-text-internal.h>

G_BEGIN_DECLS

typedef enum
{
  TPL_ENTRY_TEXT_SIGNAL_NONE = 0,
  TPL_ENTRY_TEXT_SIGNAL_SENT,
  TPL_ENTRY_TEXT_SIGNAL_RECEIVED,
  TPL_ENTRY_TEXT_SIGNAL_SEND_ERROR,
  TPL_ENTRY_TEXT_SIGNAL_LOST_MESSAGE,
  TPL_ENTRY_TEXT_SIGNAL_CHAT_STATUS_CHANGED,
  TPL_ENTRY_SIGNAL_CHANNEL_CLOSED
} TplEntryTextSignalType;

struct _TplEntryText
{
  TplEntry parent;

  /* Private */
  TplEntryTextPriv *priv;
};

struct _TplEntryTextClass
{
  TplEntryClass parent_class;
};

TplEntryText * _tpl_entry_text_new (const gchar* log_id,
    TpAccount *account,
    TplEntryDirection direction);

TpChannelTextMessageType _tpl_entry_text_message_type_from_str (
    const gchar *type_str);

const gchar * _tpl_entry_text_message_type_to_str (
    TpChannelTextMessageType msg_type);

TplChannelText * _tpl_entry_text_get_tpl_channel_text (
    TplEntryText *self);

TplEntryTextSignalType _tpl_entry_text_get_signal_type (
    TplEntryText *self);

void _tpl_entry_text_set_tpl_channel_text (TplEntryText *self,
    TplChannelText *data);

void _tpl_entry_text_set_message (TplEntryText *self,
    const gchar *data);

void _tpl_entry_text_set_message_type (TplEntryText *self,
    TpChannelTextMessageType data);

void _tpl_entry_text_set_chatroom (TplEntryText *self,
    gboolean data);

TpChannelTextMessageType _tpl_entry_text_get_message_type (
    TplEntryText *self);

gboolean _tpl_entry_text_is_chatroom (TplEntryText *self);

gboolean _tpl_entry_text_equal (TplEntry *message1,
    TplEntry *message2);

void _tpl_entry_text_set_pending_msg_id (TplEntryText *self,
    gint data);

void _tpl_entry_text_set_signal_type (TplEntryText *self,
    TplEntryTextSignalType signal_type);

gboolean _tpl_entry_text_is_pending (TplEntryText *self);

G_END_DECLS
#endif // __TPL_ENTRY_TEXT_INTERNAL_H__
