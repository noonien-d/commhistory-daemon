/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#ifndef LOCSTRINGS_H
#define LOCSTRINGS_H
/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the GNU Lesser General Public License version 2.1 as
** published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
** or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
** License for more details.
**
** You should have received a copy of the GNU Lesser General Public License
** along with this library; if not, write to the Free Software Foundation, Inc.,
** 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
******************************************************************************/

#include <MLocale>

//% "Messages"
#define txt_qtn_msg_notifications_group qtTrId("qtn_msg_notifications_group")
//% "Warnings"
#define txt_qtn_msg_errors_group qtTrId("qtn_msg_errors_group")
//% "Missed calls"
#define txt_qtn_msg_missed_calls_group qtTrId("qtn_msg_missed_calls_group")
//% "Voicemail"
#define txt_qtn_msg_voicemail_group qtTrId("qtn_msg_voicemail_group")

//% "%n new message(s)"
#define txt_qtn_msg_notification_new_message(NUM) qtTrId("qtn_msg_notification_new_message", NUM)
//% "Contact card"
#define txt_qtn_msg_contact_card_label qtTrId("qtn_msg_contact_card_label")
//% "%1 | %2"
#define txt_qtn_msg_notification_new_vcard(STR) qtTrId("qtn_msg_received_contact_card").arg(txt_qtn_msg_contact_card_label).arg(STR)
//% "Reply"
#define txt_qtn_msg_notification_reply qtTrId("qtn_msg_notification_reply")
//% "Call"
#define txt_qtn_msg_notification_call qtTrId("qtn_msg_notification_call")
//% "%n attachment(s)"
#define txt_qtn_mms_notification_attachment(NUM) qtTrId("qtn_mms_notification_attachment", NUM)
//% "%1 | %2"
//: Notification text for MMS. %1 is attachment text, %2 subject/message. e.g. "1 attachment | Kitty!"
#define txt_qtn_mms_notification_with_text(NUMATTACHMENTS,STR) qtTrId("qtn_mms_notification_with_text").arg(txt_qtn_mms_notification_attachment(NUMATTACHMENTS)).arg(STR)

//% "%n missed call(s)"
#define txt_qtn_call_missed(NUM) qtTrId("qtn_call_missed", NUM)
//% "%n voicemail(s)"
#define txt_qtn_call_voicemail_notification(NUM) qtTrId("qtn_call_voicemail_notification", NUM)
//% "Private number"
#define txt_qtn_call_type_private qtTrId("qtn_call_type_private")
//% "Voicemail"
#define txt_qtn_call_type_voicemail qtTrId("qtn_call_type_voicemail")
//% "Call back"
#define txt_qtn_call_notification_call_back qtTrId("txt_qtn_call_notification_call_back")
//% "Send message"
#define txt_qtn_call_notification_send_message qtTrId("txt_qtn_call_notification_send_message")

//% "Tap to listen"
#define txt_qtn_voicemail_prompt qtTrId("qtn_voicemail_prompt")
//% "Call voicemail"
#define txt_qtn_voicemail_notification_call qtTrId("txt_qtn_voicemail_notification_call")

//% "Multimedia message was delivered to %1"
#define txt_qtn_msg_notification_delivered(STR) qtTrId("qtn_mms_info_delivered").arg(STR)
//% "Multimedia message was read by %1"
#define txt_qtn_msg_notification_read(STR) qtTrId("qtn_mms_info_msg_read").arg(STR)
//% "Multimedia message was deleted without reading by %1"
#define txt_qtn_msg_notification_deleted(STR) qtTrId("qtn_mms_info_delete_wo_reading").arg(STR)
//% "MMS is ready to download"
#define txt_qtn_mms_notification_manual_download qtTrId("qtn_mms_notification_manual_download")
//% "MMS sending failed"
#define txt_qtn_mms_notification_send_failed qtTrId("qtn_mms_notification_send_failed")
//% "MMS downloading failed"
#define txt_qtn_mms_notification_download_failed qtTrId("qtn_mms_notification_download_failed")

//% "%1 has joined"
#define txt_qtn_msg_group_chat_remote_joined(STR) qtTrId("qtn_msg_group_chat_remote_joined").arg(STR)
//% "%1 has left"
#define txt_qtn_msg_group_chat_remote_left(STR) qtTrId("qtn_msg_group_chat_remote_left").arg(STR)
//% "%1 removed you from this chat"
#define txt_qtn_msg_group_chat_you_removed(STR) qtTrId("qtn_msg_group_chat_you_removed").arg(STR)
//% "%1 removed %2 from this chat"
#define txt_qtn_msg_group_chat_person_removed(STR1,STR2) qtTrId("qtn_msg_group_chat_person_removed").arg(STR1).arg(STR2)

//% "%1 changed room topic to %2"
#define txt_qtn_msg_group_chat_topic_changed(STR1,STR2) qtTrId("qtn_msg_group_chat_topic_changed").arg(STR1).arg(STR2)
//% "%1 removed the topic"
#define txt_qtn_msg_group_chat_topic_cleared(STR) qtTrId("qtn_msg_group_chat_topic_cleared").arg(STR)
//% "You changed the room topic to %1"
#define txt_qtn_msg_group_chat_topic_user_changed(STR) qtTrId("qtn_msg_group_chat_topic_user_changed").arg(STR)
//% "You removed the topic"
#define txt_qtn_msg_group_chat_topic_cleared_user qtTrId("qtn_msg_group_chat_topic_cleared_user")


//% "SMS sending failed to %1"
#define txt_qtn_msg_error_sms_sending_failed(STR) qtTrId("qtn_msg_error_sms_sending_failed").arg(STR)
//% "Message sending failed to %1"
#define txt_qtn_msg_error_sending_failed(STR) qtTrId("qtn_msg_error_sending_failed").arg(STR)
//% "SMS sending failed. No message center number found in the sim card."
#define txt_qtn_msg_error_missing_smsc qtTrId("qtn_msg_error_missing_smsc")
//% "Fixed dialing active. Sending not allowed to %1."
#define txt_qtn_re_error_denied_phone_number(STR) qtTrId("qtn_re_error_denied_phone_number").arg(STR)

//% "Contact is offline. Unable to guarantee that the message will be delivered."
#define txt_qtn_msg_general_supports_offline qtTrId("qtn_msg_general_supports_offline")
//% "Unable to send message. Contact is offline."
#define txt_qtn_msg_general_does_not_support_offline qtTrId("qtn_msg_general_does_not_support_offline")
//% "All participants are offline."
#define txt_qtn_msg_all_participants_offline qtTrId("qtn_msg_all_participants_offline")

//% "Authorization request %1"
#define txt_qtn_pers_authorization_req qtTrId("qtn_pers_authorization_req")
//% "You need to be online to perform this operation"
#define txt_qtn_pers_offline qtTrId("qtn_pers_offline")

//% "Group chat"
#define txt_qtn_msg_group_chat qtTrId("qtn_msg_group_chat")

#endif // LOCSTRINGS_H
