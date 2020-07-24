/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013 Jolla Ltd.
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: John Brooks <john.brooks@jolla.com>
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

#include "personalnotification.h"
#include "notificationmanager.h"
#include "notificationgroup.h"
#include "locstrings.h"
#include "constants.h"
#include "debug.h"
#include <CommHistory/commonutils.h>
#include <notification.h>
#include <MLocale>

using namespace RTComLogger;
using namespace CommHistory;

PersonalNotification::PersonalNotification(QObject* parent) : QObject(parent),
    m_eventType(CommHistory::Event::UnknownType),
    m_chatType(CommHistory::Group::ChatTypeP2P),
    m_hasPendingEvents(false),
    m_hidden(false),
    m_restored(false),
    m_notification(0)
{
}

PersonalNotification::PersonalNotification(const QString& remoteUid,
                                           const QString& account,
                                           CommHistory::Event::EventType eventType,
                                           const QString& channelTargetId,
                                           CommHistory::Group::ChatType chatType,
                                           uint contactId,
                                           const QString& lastNotification,
                                          QObject* parent) :
    QObject(parent), m_remoteUid(remoteUid), m_account(account),
    m_eventType(eventType), m_targetId(channelTargetId), m_chatType(chatType),
    m_notificationText(lastNotification),
    m_hasPendingEvents(true),
    m_hidden(false),
    m_restored(false),
    m_notification(0),
    m_recipient(account, remoteUid)
{
}

PersonalNotification::~PersonalNotification()
{
    delete m_notification;
}

bool PersonalNotification::restore(Notification *n)
{
    if (m_notification && m_notification != n) {
        delete m_notification;
        m_notification = 0;
    }

    // Support old style binary data, but use base64 normally
    QByteArray data = n->hintValue("x-commhistoryd-data").toByteArray();
    if (!data.contains((char)0))
        data = QByteArray::fromBase64(data);

    if (data.isEmpty())
        return false;

    QDataStream stream(data);
    stream.setVersion(QDataStream::Qt_5_0);
    stream >> *this;
    if (stream.status())
        return false;

    m_notification = n;
    m_recipient = Recipient(account(), remoteUid());
    m_restored = true;
    connect(m_notification, SIGNAL(closed(uint)), SLOT(onClosed(uint)));
    return true;
}

QByteArray PersonalNotification::serialized() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_5_0);
    stream << *this;
    if (stream.status())
        return QByteArray();
    return data;
}

void PersonalNotification::publishNotification()
{
    QString name;

    // voicemail notifications shouldn't have contact name
    if (m_eventType != CommHistory::Event::VoicemailEvent)
        name = notificationName();

    if (!m_notification) {
        m_notification = new Notification(this);
        connect(m_notification, SIGNAL(closed(uint)), SLOT(onClosed(uint)));

        m_notification->setTimestamp(QDateTime::currentDateTimeUtc());
    }

    m_notification->setAppName(NotificationGroup::groupName(collection()));
    m_notification->setCategory(NotificationGroup::groupType(m_eventType));
    m_notification->setHintValue("x-commhistoryd-data", serialized().toBase64());
    m_notification->setHintValue("x-nemo-hidden", m_hidden);
    m_notification->setSummary(name);
    m_notification->setBody(notificationText());
    m_notification->clearPreviewSummary();
    m_notification->clearPreviewBody();

    NotificationManager::instance()->setNotificationProperties(m_notification, this, false);

    // Show preview banner for notifications not previously reported
    // (missed calls have no preview as the incoming call dialog was just shown)
    if ((collection() != Voice) && !m_hidden && m_notification->replacesId() == 0) {
        Notification preview;

        preview.setAppName(m_notification->appName());
        preview.setCategory(m_notification->category() + QStringLiteral(".preview"));
        preview.setPreviewSummary(m_notification->summary());
        preview.setPreviewBody(m_notification->body());

        NotificationManager::instance()->setNotificationProperties(&preview, this, false);

        preview.publish();

        DEBUG() << preview.replacesId() << preview.category() << preview.previewSummary() << preview.previewBody();
    }

    m_notification->publish();

    setHasPendingEvents(false);
    m_restored = false;

    DEBUG() << m_notification->replacesId() << m_notification->category() << m_notification->summary() << m_notification->body() << m_notification->hintValue("x-nemo-hidden");
}

void PersonalNotification::removeNotification()
{
    DEBUG() << "removing notification" << m_notification;
    if (m_notification) {
        m_notification->close();
        m_notification->deleteLater();
        m_notification = 0;
    }

    setHasPendingEvents(false);
}

QString PersonalNotification::notificationName() const
{
    if (!chatName().isEmpty()) {
        return chatName();
    } else if (!contactName().isEmpty()) {
        return contactName();
    } else if (remoteUid() == QLatin1String("<hidden>")) {
        return txt_qtn_call_type_private;
    } else if (CommHistory::localUidComparesPhoneNumbers(account())) {
        ML10N::MLocale locale;
        return locale.toLocalizedNumbers(remoteUid());
    } else
        return remoteUid();
}

PersonalNotification::EventCollection PersonalNotification::collection() const
{
    return collection(m_eventType);
}

PersonalNotification::EventCollection PersonalNotification::collection(uint eventType)
{
    if (eventType == CommHistory::Event::VoicemailEvent)
        return Voicemail;
    if (eventType == CommHistory::Event::CallEvent)
        return Voice;
    return Messaging;
}

QString PersonalNotification::remoteUid() const
{
    return m_remoteUid;
}

QString PersonalNotification::account() const
{
    return m_account;
}

uint PersonalNotification::eventType() const
{
    return m_eventType;
}

QString PersonalNotification::targetId() const
{
    return m_targetId;
}

uint PersonalNotification::chatType() const
{
    return m_chatType;
}

QString PersonalNotification::contactName() const
{
    return m_recipient.displayName();
}

uint PersonalNotification::contactId() const
{
    return m_recipient.contactId();
}


QString PersonalNotification::notificationText() const
{
    return m_notificationText;
}

bool PersonalNotification::hasPendingEvents() const
{
    return m_hasPendingEvents;
}

QString PersonalNotification::chatName() const
{
    return m_chatName;
}

QString PersonalNotification::eventToken() const
{
    return m_eventToken;
}

QString PersonalNotification::smsReplaceNumber() const
{
    return m_smsReplaceNumber;
}

QDateTime PersonalNotification::timestamp() const
{
    if (m_notification)
        return m_notification->timestamp();

    return QDateTime();
}

bool PersonalNotification::hidden() const
{
    return m_hidden;
}

bool PersonalNotification::restored() const
{
    return m_restored;
}

void PersonalNotification::setRemoteUid(const QString& remoteUid)
{
    if (m_remoteUid != remoteUid) {
        m_remoteUid = remoteUid;
        setHasPendingEvents(true);
    }
}

void PersonalNotification::setAccount(const QString& account)
{
    if (m_account != account) {
        m_account = account;
        setHasPendingEvents(true);
    }
}

void PersonalNotification::setEventType(uint eventType)
{
    if (m_eventType != eventType) {
        m_eventType = eventType;
        setHasPendingEvents(true);
    }
}

void PersonalNotification::setTargetId(const QString& targetId)
{
    if (m_targetId != targetId) {
        m_targetId = targetId;
        setHasPendingEvents(true);
    }
}

void PersonalNotification::setChatType(uint chatType)
{
    if (m_chatType != chatType) {
        m_chatType = chatType;
        setHasPendingEvents(true);
    }
}

void PersonalNotification::setNotificationText(const QString& notificationText)
{
    if (m_notificationText != notificationText) {
        m_notificationText = notificationText;
        setHasPendingEvents(true);
    }
}

void PersonalNotification::setHasPendingEvents(bool hasPendingEvents)
{
    if (m_hasPendingEvents != hasPendingEvents) {
        m_hasPendingEvents = hasPendingEvents;
        emit hasPendingEventsChanged(hasPendingEvents);
    }
}

void PersonalNotification::setChatName(const QString& chatName)
{
    if (m_chatName != chatName) {
        m_chatName = chatName;
        setHasPendingEvents(true);
    }
}

void PersonalNotification::setEventToken(const QString &eventToken)
{
    if (m_eventToken != eventToken) {
        m_eventToken = eventToken;
        setHasPendingEvents(true);
    }
}

void PersonalNotification::setSmsReplaceNumber(const QString &number)
{
    if (m_smsReplaceNumber != number) {
        m_smsReplaceNumber = number;
        setHasPendingEvents(true);
    }
}

void PersonalNotification::setHidden(bool hide)
{
    if (m_hidden != hide) {
        m_hidden = hide;
        setHasPendingEvents(true);
    }
}

const Recipient &PersonalNotification::recipient() const
{
    return m_recipient;
}

void PersonalNotification::updateRecipientData()
{
    setHasPendingEvents(true);
}

void PersonalNotification::onClosed(uint /*reason*/)
{
    emit notificationClosed(this);
}

QDataStream& operator<<(QDataStream &out, const RTComLogger::PersonalNotification &key)
{
    key.serialize(out, key);
    return out;
}

QDataStream& operator>>(QDataStream &in, RTComLogger::PersonalNotification &key)
{
    key.deSerialize(in, key);

    // Hidden property is not in the serialization from earlier forms
    if (!key.property("hidden").isValid()) {
        key.setProperty("hidden", QVariant::fromValue(false));
    }

    return in;
}

