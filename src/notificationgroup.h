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

#ifndef NOTIFICATIONGROUP_H
#define NOTIFICATIONGROUP_H

#include "personalnotification.h"

#include <QObject>
#include <QString>
#include <QMetaType>
#include <QTimer>

class Notification;

namespace CommHistory {
    class Event;
}

namespace RTComLogger {

class NotificationGroup : public QObject
{
    Q_OBJECT

public:
    NotificationGroup(PersonalNotification::EventCollection collection, const QString &localUid, const QString &remoteUid, QObject *parent = 0);
    virtual ~NotificationGroup();

    static QString groupType(int eventType);
    static int eventType(const QString &groupType);

    static QString groupName(PersonalNotification::EventCollection collection);
    static QString groupCategory(PersonalNotification::EventCollection collection);

    PersonalNotification::EventCollection collection() const;
    const QString &localUid() const;
    const QString &remoteUid() const;

    Notification *notification();
    QList<PersonalNotification*> notifications() const;

    /* Add a notification to this group
     *
     * Ownership and management of the notification are assumed. If marked as pending, the
     * notification will be emitted automatically. Changes to the notification will be handled
     * automatically. */
    void addNotification(PersonalNotification *notification);

    /* Remove a notification
     *
     * The notification will be unpublished and its instance deleted. The group may be empty
     * afterwards, in which case it will also be unpublished. Caller should delete this instance
     * when empty afterwards, if desired. */
    bool removeNotification(PersonalNotification *&notification);

public slots:
    /* Update the group's message text and publish it if necessary. Should not need to be called
     * manually; the group will be updated after all relevant changes to notifications. */
    void updateGroup();
    void updateGroupLater();

    /* Remove the group and all notifications. Equivalent to calling removeNotification for each
     * notification. */
    void removeGroup();

signals:
    /* Emitted when the group or any notification within it has changed */
    void changed();

private slots:
    void onNotificationChanged();
    void onNotificationClosed(PersonalNotification *);
    void onClosed(uint);

private:
    PersonalNotification::EventCollection m_collection;
    QString m_localUid;
    QString m_remoteUid;
    Notification *mGroup;
    QList<PersonalNotification*> mNotifications;
    QTimer updateTimer;

    friend class Ut_NotificationManager;

    QStringList contactNames();
    QString notificationGroupText();
    int countConversations();
};

} // namespace

#endif // NOTIFICATIONGROUP_H
