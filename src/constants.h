/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2015 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@jolla.com>
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

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <CommHistory/Event>

namespace RTComLogger {

// Used to generate duiremoteaction strings
#define OBJECT_PATH QLatin1String("/")

#define COMM_HISTORY_DAEMON_SERVICE_NAME QLatin1String("org.nemomobile.CommHistory")
#define COMM_HISTORY_DAEMON_OBJECT_PATH  QLatin1String("/org/nemomobile/CommHistory")
#define COMM_HISTORY_DAEMON_INTERFACE    QLatin1String("org.nemomobile.CommHistoryIf")

#define ACCOUNT_PRESENCE_SERVICE_NAME    QLatin1String("org.nemomobile.AccountPresence")
#define ACCOUNT_PRESENCE_OBJECT_PATH     QLatin1String("/org/nemomobile/AccountPresence")
#define ACCOUNT_PRESENCE_INTERFACE       QLatin1String("org.nemomobile.AccountPresenceIf")

#define MESSAGING_SERVICE_NAME    QLatin1String("org.nemomobile.qmlmessages")
#define MESSAGING_INTERFACE       QLatin1String("org.nemomobile.qmlmessages")
#define SHOW_INBOX_METHOD         QLatin1String("showGroupsWindow")
#define START_CONVERSATION_METHOD QLatin1String("startConversation")

#define CALL_HISTORY_SERVICE_NAME QLatin1String("com.nokia.telephony.callhistory")
#define CALL_HISTORY_OBJECT_PATH  QLatin1String("/org/maemo/m")
#define CALL_HISTORY_INTERFACE    QLatin1String("com.nokia.MApplicationIf")
#define CALL_HISTORY_METHOD       QLatin1String("launch")
#define CALL_HISTORY_PARAMETER    QLatin1String("callHistory")

#define VOICEMAIL_INTERFACE    QLatin1String("com.nokia.telephony.callhistory")
#define VOICEMAIL_OBJECT_PATH  QLatin1String("/callhistory")
#define VOICEMAIL_METHOD       QLatin1String("voicemail")
#define REPLACE_TYPE           QLatin1String("sms-replace-number")

#define VOICECALL_SERVICE      QLatin1String("com.jolla.voicecall.ui")
#define VOICECALL_OBJECT_PATH  QLatin1String("/")
#define VOICECALL_INTERFACE    QLatin1String("com.jolla.voicecall.ui")
#define VOICECALL_DIAL_METHOD  QLatin1String("dial")

// Custom hints for identifying notifications
#define ACCOUNT_PATH_HINT   QLatin1String("x-org-nemomobile-qmlmessages.account.path")
#define CONTACT_ID_HINT     QLatin1String("x-org-nemomobile-qmlmessages.contact.id")

#define RING_ACCOUNT_PATH_PREFIX QLatin1String("/org/freedesktop/Telepathy/Account/ring/tel")

#define CONTACT_SEPARATOR_IN_NOTIFICATION_GROUP QLatin1String(", ")

#define TELEPATHY_CHANNEL_INTERFACE_PERSISTENT_ID QLatin1String("com.nokia.Telepathy.Channel.Interface.Persistent.PersistentID")

// notification threshold is 3 seconds
#define NOTIFICATION_THRESHOLD 3000
// threshold to trigger contact request after added/modified contacts signals
#define CONTACT_REQUEST_THRESHOLD 5000
// give up on contact fetch request
#define CONTACT_REQUEST_TIMEROUT 3000
/* Clean up check -period for old calls as DAYS.
   Note: this cannot be > 24 days, because then the int value given for QTimer::setInterval(int)
   will go out of int range. */
#define CLEANUP_PERIOD_DAYS 7
// First clean up after boot in ms.
#define BOOT_CLEANUP_MS 3600000
// This amount of days is counted back to determine how old calls should be deleted.
#define REMOVAL_TARGET_DAYS -90

// Voice mail contact's GUID, used by MeeGo Harmattan Contacts application.
#define VOICEMAIL_CONTACT_GUID QLatin1String("9aeeea49-fab9-4d4e-b0cc-497f2aeac9a8")

// This event type is used only locally (in NotificationManager) and is not defined in CommHistory::Event::EvenType enum.
#define VOICEMAIL_SMS_EVENT_TYPE 100

// Path and name of the vmid file used to contain local id of of a voice mail contact when such a contact is added.
#define VOICEMAIL_CONTACT_VMID_MAIN QLatin1String("/dev/shm")
#define VOICEMAIL_CONTACT_VMID_DIR  QLatin1String("contacts")
#define VOICEMAIL_CONTACT_VMID_FILE QLatin1String("vmid")

// events
struct EventTypes {
    int type;
    const char* event;
};

static const EventTypes _eventTypes[] =
{
    {CommHistory::Event::IMEvent,       "x-nemo.messaging.im"},
    {CommHistory::Event::SMSEvent,      "x-nemo.messaging.sms"},
    {CommHistory::Event::MMSEvent,      "x-nemo.messaging.mms"},
    {CommHistory::Event::CallEvent,     "x-nemo.call.missed"},
    {CommHistory::Event::VoicemailEvent,"x-nemo.messaging.voicemail"},
    {VOICEMAIL_SMS_EVENT_TYPE,          "x-nemo.messaging.voicemail-SMS"}
};

static const int _eventTypesCount = sizeof(_eventTypes) / sizeof(EventTypes);

// Custom system info notification types for commhistoryd:
const QString ErrorCategory = "x-nemo.messaging.error";
const QString StrongErrorCategory = "x-nemo.messaging.error.strong";

const QString voicemailWaitingCategory = "x-nemo.messaging.voicemail-waiting";

}

#endif //#define CONSTANTS_H
