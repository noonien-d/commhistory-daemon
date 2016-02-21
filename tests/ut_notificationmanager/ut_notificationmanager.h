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

#ifndef UT_NOTIFICATIONMANAGER_H
#define UT_NOTIFICATIONMANAGER_H

// INCLUDES
#include <QObject>

#include <CommHistory/Event>
#include "notificationmanager.h"

namespace RTComLogger {

class PersonalNotification;

class Ut_NotificationManager : public QObject
{
    Q_OBJECT
public:
    Ut_NotificationManager();
    ~Ut_NotificationManager();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

// Test functions
private Q_SLOTS:
    void testShowNotification();
    void groupNotifications();

private:
    NotificationManager* nm;
    int eventId;

    CommHistory::Event createEvent(CommHistory::Event::EventType type, const QString &remoteUid, const QString &localUid);
    PersonalNotification *getNotification(const CommHistory::Event &event);
    NotificationGroup *getGroup(const CommHistory::Event &event);
};

}
#endif // UT_NOTIFICATIONMANAGER_H
