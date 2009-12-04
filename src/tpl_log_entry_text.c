#include <tpl_channel_data.h>
#include <tpl_contact.h>
#include <tpl_log_entry_text.h>
#include <tpl_utils.h>

G_DEFINE_TYPE (TplLogEntryText, tpl_log_entry_text, G_TYPE_OBJECT)

static void tpl_log_entry_text_class_init(TplLogEntryTextClass* klass) {
	//GObjectClass* gobject_class = G_OBJECT_CLASS (klass);
}

static void tpl_log_entry_text_init(TplLogEntryText* self) {
#define TPL_SET_NULL(x) tpl_log_entry_text_set_##x(self, NULL)
	TPL_SET_NULL(tpl_channel);
#undef TPL_SET_NULL
}

TplLogEntryText *tpl_log_entry_text_new(void) {
	TplLogEntryText *ret = g_object_new(TPL_TYPE_LOG_ENTRY_TEXT, NULL);
	return ret;
}



const gchar *tpl_log_entry_text_message_type_to_str (TpChannelTextMessageType msg_type)
{
       switch (msg_type) {
        case TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION:
                return "action";
        case TP_CHANNEL_TEXT_MESSAGE_TYPE_NOTICE:
                return "notice";
        case TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY:
                return "auto-reply";
        default:
                return "normal";
        }
}


TplChannel *tpl_log_entry_text_get_tpl_channel(TplLogEntryText *self) {
	return self->tpl_channel;
}
TplContact *tpl_log_entry_text_get_sender (TplLogEntryText *self) {
	return self->sender;
}
TplContact *tpl_log_entry_text_get_receiver (TplLogEntryText *self) {
	return self->receiver;
}
const gchar *tpl_log_entry_text_get_message (TplLogEntryText *self) {
	return self->message;
}
TpChannelTextMessageType tpl_log_entry_text_get_message_type (TplLogEntryText *self) {
	return self->message_type;
}
TplLogEntryTextSignalType tpl_log_entry_text_get_signal_type (TplLogEntryText *self) {
	return self->signal_type;
}
TplLogEntryTextDirection tpl_log_entry_text_get_direction (TplLogEntryText *self) {
	return self->direction;
}
time_t tpl_log_entry_text_get_timestamp (TplLogEntryText *self)
{
	return self->timestamp;
}
guint tpl_log_entry_text_get_id (TplLogEntryText *self)
{
	return self->id;
}


void tpl_log_entry_text_set_tpl_channel(TplLogEntryText *self, TplChannel *data) {
	_unref_object_if_not_null(self->tpl_channel);
	self->tpl_channel = data;
	_ref_object_if_not_null(data);
}

void tpl_log_entry_text_set_sender (TplLogEntryText *self, TplContact *data) {
	self->sender = data;
}
void tpl_log_entry_text_set_receiver (TplLogEntryText *self, TplContact *data) {
	self->receiver = data;
}
void tpl_log_entry_text_set_message (TplLogEntryText *self, const gchar *data) {
	self->message = data;
}
void tpl_log_entry_text_set_message_type (TplLogEntryText *self, TpChannelTextMessageType data) {
	self->message_type = data;
}
void tpl_log_entry_text_set_signal_type (TplLogEntryText *self, TplLogEntryTextSignalType data) {
	self->signal_type = data;
}
void tpl_log_entry_text_set_direction (TplLogEntryText *self, TplLogEntryTextDirection data) {
	self->direction = data;
}

void tpl_log_entry_text_set_timestamp (TplLogEntryText *self, time_t data)
{
	self->timestamp = data;
}

void tpl_log_entry_text_set_id (TplLogEntryText *self, guint data)
{
	self->id = data;
}
