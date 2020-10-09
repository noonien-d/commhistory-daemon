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

#ifndef PERSONALNOTIFICATION_H
#define PERSONALNOTIFICATION_H

#include <QObject>
#include <QString>
#include <QMetaType>

#include "serialisable.h"

// commhistory
#include <CommHistory/Group>
#include <CommHistory/Recipient>

class Notification;

namespace RTComLogger {

class PersonalNotification : public QObject, public Serialisable
{
    Q_OBJECT

    // These properties determine the serialization of this object; if you change
    // them in any way, you must account for it in the serialization functions

    Q_PROPERTY(QString remoteuid READ remoteUid WRITE setRemoteUid)
    Q_PROPERTY(QString account READ account WRITE setAccount)

    // can't use CommHistory::Event::EventType because it cannot be
    // properly declared as a Q_ENUM, because CommHistory::Event is not a
    // QObject.
    Q_PROPERTY(uint eventType READ eventType WRITE setEventType)
    Q_PROPERTY(QString targetId READ targetId WRITE setTargetId)
    // ...and the same goes for Group::ChatType.
    Q_PROPERTY(uint chatType READ chatType WRITE setChatType)

    Q_PROPERTY(uint contactid READ contactId)
    Q_PROPERTY(QString notificationtext READ notificationText
                                        WRITE setNotificationText)
    Q_PROPERTY(bool haspendingevents READ hasPendingEvents
                                     WRITE setHasPendingEvents)
    Q_PROPERTY(QString chatName READ chatName
                                WRITE setChatName)
    Q_PROPERTY(QString eventToken READ eventToken
                                  WRITE setEventToken)
    Q_PROPERTY(QString smsReplaceNumber READ smsReplaceNumber
                                        WRITE setSmsReplaceNumber)
    Q_PROPERTY(bool hidden READ hidden
                           WRITE setHidden)

public:
    enum EventCollection { Messaging = 0, Voicemail, Voice };

    PersonalNotification(QObject* parent = 0);
    PersonalNotification(const QString& remoteUid,
                         const QString& account,
                         CommHistory::Event::EventType eventType = CommHistory::Event::UnknownType,
                         const QString& targetId = QString(),
                         CommHistory::Group::ChatType chatType = CommHistory::Group::ChatTypeP2P,
                         uint contactId = 0,
                         const QString& lastNotification = QString(),
                         QObject* parent = 0);

    virtual ~PersonalNotification();

    bool restore(Notification *notification);

    void publishNotification();
    void removeNotification();

    Notification *notification() const { return m_notification; }

    QString notificationName() const;

    EventCollection collection() const;
    static EventCollection collection(uint eventType);

    QString remoteUid() const;
    QString account() const;
    uint eventType() const;
    /* remoteUid = sender of the message, targetId = channel target id.
       In P2P conversations they are the same, in Skype MUC targetId =
       persistent room id. TargetId is used for Telepathy channel
       requests. */
    QString targetId() const;
    uint chatType() const;
    uint contactId() const;
    QString contactName() const;
    QString notificationText() const;
    bool hasPendingEvents() const;
    QString chatName() const;
    QString eventToken() const;
    QString smsReplaceNumber() const;
    QDateTime timestamp() const;
    bool hidden() const;

    bool hasPhoneNumber() const;

    void setRemoteUid(const QString& remoteUid);
    void setAccount(const QString& account);
    void setEventType(uint eventType);
    void setTargetId(const QString& targetId);
    void setChatType(uint chatType);
    void setNotificationText(const QString& notificationText);
    void setHasPendingEvents(bool hasPendingEvents = true);
    void setChatName(const QString& chatName);
    void setEventToken(const QString& eventToken);
    void setSmsReplaceNumber(const QString& number);
    void setHidden(bool hide = true);

    const CommHistory::Recipient &recipient() const;

    void updateRecipientData();

signals:
    void hasPendingEventsChanged(bool hasPendingEvents);
    void notificationClosed(PersonalNotification *);

private slots:
    void onClosed(uint);

private:
    QString m_remoteUid;
    QString m_account;
    uint m_eventType;
    QString m_targetId;
    uint m_chatType;
    QString m_notificationText;
    bool m_hasPendingEvents;
    QString m_chatName;
    QString m_eventToken;
    QString m_smsReplaceNumber;
    bool m_hidden;
    bool m_restored;

    Notification *m_notification;
    CommHistory::Recipient m_recipient;

    QByteArray serialized() const;
};

} // namespace

QDataStream& operator<<(QDataStream &out, const RTComLogger::PersonalNotification &key);
QDataStream& operator>>(QDataStream &in, RTComLogger::PersonalNotification &key);

#endif // PERSONALNOTIFICATION_H
