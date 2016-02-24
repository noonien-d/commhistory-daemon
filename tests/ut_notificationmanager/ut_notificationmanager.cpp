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

// INCLUDES
#include "ut_notificationmanager.h"
#include "locstrings.h"
#include "constants.h"

// Qt includes
#include <QDebug>
#include <QTest>
#include <QDateTime>

#include <notification.h>

#define CONTACT_1_REMOTE_ID QLatin1String("td@localhost")
#define CONTACT_2_REMOTE_ID QLatin1String("td2@localhost")
#define DUT_ACCOUNT_PATH QLatin1String("/org/freedesktop/Telepathy/Account/gabble/jabber/dut_40localhost0")
#define MESSAGE_TEXT QLatin1String("Testing notifications!")
#define RING_ACCOUNT_PATH "/org/freedesktop/Telepathy/Account/ring/tel/"

using namespace RTComLogger;
using namespace CommHistory;

Ut_NotificationManager::Ut_NotificationManager() : eventId(1)
{
}

Ut_NotificationManager::~Ut_NotificationManager()
{
}

/*!
 * This function will be called before the first testfunction is executed.
 */
void Ut_NotificationManager::initTestCase()
{
    nm = NotificationManager::instance();
    nm->m_Groups.clear();
}

/*!
 * This function will be called after the last testfunction was executed.
 */
void Ut_NotificationManager::cleanupTestCase()
{
    delete nm;
    nm = 0;
}

/*!
 * This function will be called before each testfunction is executed.
 */
void Ut_NotificationManager::init()
{
}

/*!
 * This unction will be called after every testfunction.
 */
void Ut_NotificationManager::cleanup()
{
}

CommHistory::Event Ut_NotificationManager::createEvent(CommHistory::Event::EventType type, const QString &remoteUid, const QString &localUid = DUT_ACCOUNT_PATH)
{
    eventId++;
    CommHistory::Event event;
    event.setType(type);
    event.setDirection(CommHistory::Event::Inbound);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(QDateTime::currentDateTime());
    event.setLocalUid(localUid);
    event.setRecipients(Recipient(localUid, remoteUid));

    if (type == CommHistory::Event::IMEvent || type == CommHistory::Event::SMSEvent) {
        event.setFreeText(MESSAGE_TEXT);
        event.setMessageToken(MESSAGE_TEXT + QString::number(eventId));
        event.setGroupId(1);
    } else if (type == CommHistory::Event::CallEvent) {
        event.setIsMissedCall(true);
    }

    event.setId(eventId);
    return event;
}

PersonalNotification *Ut_NotificationManager::getNotification(const CommHistory::Event &event)
{
    NotificationManager::EventGroupProperties groupProperties(NotificationManager::eventGroup(PersonalNotification::collection(event.type()), event.recipients().value(0)));
    NotificationGroup *group = nm->m_Groups.value(groupProperties);
    foreach (PersonalNotification *pn, group->notifications()) {
        if (pn->eventToken() == event.messageToken())
            return pn;
    }
    return 0;
}

NotificationGroup *Ut_NotificationManager::getGroup(const CommHistory::Event &event)
{
    NotificationManager::EventGroupProperties groupProperties(NotificationManager::eventGroup(PersonalNotification::collection(event.type()), event.recipients().value(0)));
    return nm->m_Groups.value(groupProperties);
}

void Ut_NotificationManager::testShowNotification()
{
    CommHistory::Event event = createEvent(CommHistory::Event::IMEvent, CONTACT_1_REMOTE_ID);
    nm->showNotification(event, CONTACT_1_REMOTE_ID);
    QVERIFY(nm->pendingEventCount() > 0);
    QTRY_COMPARE(nm->pendingEventCount(), 0);

    PersonalNotification *pn = getNotification(event);
    QVERIFY(pn);
    QTRY_VERIFY(!pn->hasPendingEvents());
    QVERIFY(pn->notificationName().contains(CONTACT_1_REMOTE_ID));
    QCOMPARE(pn->notificationText(), MESSAGE_TEXT);

    Notification *n = pn->notification();
    QTRY_VERIFY(n);
    QTRY_VERIFY(n->replacesId() > 0);

    NotificationGroup *group = getGroup(event);
    Notification *groupNotification = group->notification();
    QVERIFY(groupNotification);
    QVERIFY(groupNotification->replacesId() > 0);
}

void Ut_NotificationManager::groupNotifications()
{
    CommHistory::Event event = createEvent(CommHistory::Event::SMSEvent, "12345678", RING_ACCOUNT_PATH "account0");
    nm->showNotification(event, "12345678");
    QTRY_COMPARE(nm->pendingEventCount(), 0);

    NotificationGroup *group1 = getGroup(event);
    QVERIFY(group1 != 0);
    QCOMPARE(group1->contactNames(), QStringList() << QString("12345678"));

    event = createEvent(CommHistory::Event::SMSEvent, "23456789", RING_ACCOUNT_PATH "account0");
    nm->showNotification(event, "23456789");
    QTRY_COMPARE(nm->pendingEventCount(), 0);

    NotificationGroup *group2 = getGroup(event);
    QVERIFY(group2 != 0);
    QVERIFY(group2 != group1);
    QCOMPARE(group2->contactNames(), QStringList() << QString("23456789"));

    event = createEvent(CommHistory::Event::SMSEvent, "+0123456789", RING_ACCOUNT_PATH "account0");
    nm->showNotification(event, "+0123456789");
    QTRY_COMPARE(nm->pendingEventCount(), 0);

    NotificationGroup *group3 = getGroup(event);
    QVERIFY(group3 != 0);
    QCOMPARE(group3, group2);
    QCOMPARE(group3->contactNames(), QStringList() << QString("+0123456789"));

    event = createEvent(CommHistory::Event::SMSEvent, "23456789", RING_ACCOUNT_PATH "account1");
    nm->showNotification(event, "23456789");
    QTRY_COMPARE(nm->pendingEventCount(), 0);

    NotificationGroup *group4 = getGroup(event);
    QVERIFY(group4 != 0);
    QCOMPARE(group4, group2);
    QCOMPARE(group4->contactNames(), QStringList() << QString("+0123456789"));

    event = createEvent(CommHistory::Event::SMSEvent, "+1012345678", RING_ACCOUNT_PATH "account1");
    nm->showNotification(event, "+1012345678");
    QTRY_COMPARE(nm->pendingEventCount(), 0);

    NotificationGroup *group5 = getGroup(event);
    QVERIFY(group5 != 0);
    QCOMPARE(group5, group1);
    QCOMPARE(group5->contactNames(), QStringList() << QString("+1012345678"));

    event = createEvent(CommHistory::Event::SMSEvent, "012345678", RING_ACCOUNT_PATH "account1");
    nm->showNotification(event, "012345678");
    QTRY_COMPARE(nm->pendingEventCount(), 0);

    NotificationGroup *group6 = getGroup(event);
    QVERIFY(group6 != 0);
    QCOMPARE(group6, group1);
    QCOMPARE(group6->contactNames(), QStringList() << QString("+1012345678"));
}

QTEST_MAIN(Ut_NotificationManager)
