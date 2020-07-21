/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2020 Open Mobile Platform LLC.
** Copyright (C) 2013 - 2019 Jolla Ltd.
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

#include "notificationgroup.h"
#include "personalnotification.h"
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"
#include "debug.h"

#include <notification.h>

#include <MLocale>

#include <CommHistory/commonutils.h>
#include <CommHistory/Event>

using namespace RTComLogger;

ML10N::MLocale mLocale;

NotificationGroup::NotificationGroup(PersonalNotification::EventCollection collection, const QString &localUid, const QString &remoteUid, QObject *parent)
    : QObject(parent), m_collection(collection), m_localUid(localUid), m_remoteUid(remoteUid), mGroup(0)
{
    updateTimer.setInterval(0);
    updateTimer.setSingleShot(true);
    connect(&updateTimer, SIGNAL(timeout()), SLOT(updateGroup()));

    connect(this, SIGNAL(changed()), SLOT(updateGroupLater()));
}

NotificationGroup::~NotificationGroup()
{
    qDeleteAll(mNotifications);
    delete mGroup;
}

QString NotificationGroup::groupType(int eventType)
{
    for (int i = 0; i < _eventTypesCount; i++) {
        if (_eventTypes[i].type == eventType)
            return QLatin1String(_eventTypes[i].event);
    }
    return QString();
}

int NotificationGroup::eventType(const QString &groupType)
{
    for (int i = 0; i < _eventTypesCount; i++) {
        if (_eventTypes[i].event == groupType)
            return _eventTypes[i].type;
    }
    return -1;
}

QString NotificationGroup::groupName(PersonalNotification::EventCollection collection)
{
    switch (collection) {
        case PersonalNotification::Voicemail:
            return txt_qtn_msg_voicemail_group;

        case PersonalNotification::Voice:
            return txt_qtn_msg_missed_calls_group;

        case PersonalNotification::Messaging:
            return txt_qtn_msg_notifications_group;
    }

    return QString();
}

QString NotificationGroup::groupCategory(PersonalNotification::EventCollection collection)
{
    switch (collection) {
        case PersonalNotification::Voicemail:
            return QStringLiteral("x-nemo.messaging.voicemail.group");

        case PersonalNotification::Voice:
            return QStringLiteral("x-nemo.call.missed.group");

        case PersonalNotification::Messaging:
            return QStringLiteral("x-nemo.messaging.group");
    }

    return QString();
}

PersonalNotification::EventCollection NotificationGroup::collection() const
{
    return m_collection;
}

const QString &NotificationGroup::localUid() const
{
    return m_localUid;
}

const QString &NotificationGroup::remoteUid() const
{
    return m_remoteUid;
}

Notification *NotificationGroup::notification()
{
    if (!mGroup && !mNotifications.isEmpty())
        updateGroup();
    return mGroup;
}

QList<PersonalNotification*> NotificationGroup::notifications() const
{
    return mNotifications;
}

void NotificationGroup::updateGroup()
{
    if (mNotifications.isEmpty()) {
        removeGroup();
        return;
    }

    // Publish group notification, not including preview banners/sounds.
    if (!mGroup) {
        mGroup = new Notification(this);
        connect(mGroup, SIGNAL(closed(uint)), SLOT(onClosed(uint)));
    }

    const QString body(notificationGroupText());

    mGroup->setAppName(groupName(m_collection));
    mGroup->setCategory(groupCategory(m_collection));
    mGroup->setSummary(mLocale.joinStringList(contactNames()));
    if (m_collection != PersonalNotification::Voice
            && m_collection != PersonalNotification::Voicemail) {
        // For missed calls and voicemail, the Events view notification is compressed into one
        // line with only the summary, as the body information is duplicated in the notification
        // group header ('missed calls' or 'new voicemails').
        mGroup->setBody(body);
    }
    mGroup->setItemCount(mNotifications.size());

    // This group is only visible if the members are hidden
    const bool membersHidden(mNotifications[0]->hidden());
    mGroup->setHintValue("x-nemo-hidden", !membersHidden);

    const bool grouped(countConversations() > 1);
    NotificationManager::instance()->setNotificationProperties(mGroup, mNotifications[0], grouped);

    // Find the most recent timestamp from grouped notifications
    QDateTime groupTimestamp;
    bool allRestored = true;

    foreach (PersonalNotification *pn, mNotifications) {
        // Are all members restored from storage?
        allRestored &= pn->restored();

        if (pn->hasPendingEvents()) {
            // Publish this notification to ensure it has a timestamp
            pn->publishNotification();
        }

        QDateTime timestamp(pn->timestamp());
        if (groupTimestamp.isNull() || timestamp > groupTimestamp) {
            groupTimestamp = timestamp;
        }
    }
    mGroup->setTimestamp(groupTimestamp);

    // Show preview banner for this group update unless we've just restored from storage
    // (missed calls have no preview as the incoming call dialog was just shown)
    if ((m_collection != PersonalNotification::Voice) && membersHidden && !allRestored) {
        Notification preview;

        preview.setAppName(mGroup->appName());
        preview.setCategory(mGroup->category() + QStringLiteral(".preview"));
        preview.setPreviewSummary(mGroup->summary());
        preview.setPreviewBody(body);

        NotificationManager::instance()->setNotificationProperties(&preview, mNotifications[0], grouped);

        preview.publish();

        DEBUG() << preview.replacesId() << preview.category() << preview.previewSummary() << preview.previewBody();
    }

    mGroup->publish();

    DEBUG() << mGroup->replacesId() << mGroup->category() << mGroup->summary() << mGroup->body() << mGroup->hintValue("x-nemo-hidden");
}

void NotificationGroup::updateGroupLater()
{
    updateTimer.start();
}

QStringList NotificationGroup::contactNames()
{
    QStringList names;
    QList<QPair<CommHistory::Recipient, QString> > details;

    foreach (PersonalNotification *pn, mNotifications) {
        // events are ordered from most recent to oldest
        const QString &name(pn->notificationName());
        const CommHistory::Recipient &recipient(pn->recipient());

        QList<QPair<CommHistory::Recipient, QString> >::iterator it = details.begin(), end = details.end();
        for ( ; it != end; ++it) {
            if (recipient.matches((*it).first)) {
                // These events have the same recipient - use the longer name (this is for
                // the case where both names are variants of the same phone number)
                if (name.length() > (*it).second.length()) {
                    (*it).second = name;
                }
                break;
            }
        }
        if (it == details.end()) {
            details.append(qMakePair(recipient, name));
        }
    }

    // Reverse the order of the notifications
    QList<QPair<CommHistory::Recipient, QString> >::const_iterator begin = details.begin(), it = details.end();
    while (it != begin) {
        --it;
        names.append((*it).second);
    }

    return names;
}

int NotificationGroup::countConversations()
{
    QSet<QPair<QString, QString> > seen;
    foreach (PersonalNotification *pn, mNotifications)
        seen.insert(qMakePair(pn->account(), pn->remoteUid()));
    return seen.count();
}

QString NotificationGroup::notificationGroupText()
{
    QString message;
    int notifications = mNotifications.size();
    if (!notifications)
        return QString();

    switch (m_collection)
    {
        case PersonalNotification::Messaging:
        {
            if (notifications > 1)
                message = txt_qtn_msg_notification_new_message(notifications);
            else
                message = mNotifications[0]->notificationText();
            break;
        }
        case PersonalNotification::Voice:
        {
            message = txt_qtn_call_missed(notifications);
            break;
        }
        case PersonalNotification::Voicemail:
        {
            // The amount of new / not listened voicemails
            message = mNotifications[0]->notificationText();
            break;
        }
        default:
            break;
    }

    return message;
}

void NotificationGroup::removeGroup()
{
    if (mGroup) {
        mGroup->close();
        mGroup->deleteLater();
        mGroup = 0;
    }

    while (!mNotifications.isEmpty())
        removeNotification(mNotifications.first());
}

void NotificationGroup::addNotification(PersonalNotification *notification)
{
    if (mNotifications.contains(notification))
        return;

    // If notification->hasPendingEvents, the updateGroup slot will also publish the notification
    connect(notification, SIGNAL(hasPendingEventsChanged(bool)), SLOT(onNotificationChanged()));
    connect(notification, SIGNAL(notificationClosed(PersonalNotification*)), SLOT(onNotificationClosed(PersonalNotification*)));
    mNotifications.append(notification);

    // Only missed call and voicemail notifications are grouped
    if (m_collection == PersonalNotification::Voice ||
        m_collection == PersonalNotification::Voicemail) {
        if (mNotifications.count() > 1) {
            // Hide the member notification
            notification->setHidden(true);

            // Also hide the first member, which would not have been hidden on addition
            mNotifications.first()->setHidden(true);
        } else {
            // Ensure the notification is visible
            notification->setHidden(false);
        }
    }

    emit changed();
}

bool NotificationGroup::removeNotification(PersonalNotification *&notification)
{
    if (mNotifications.removeOne(notification)) {
        notification->removeNotification();
        notification->deleteLater();
        notification = 0;

        if (m_collection == PersonalNotification::Voice ||
            m_collection == PersonalNotification::Voicemail) {
            if (mNotifications.count() == 1) {
                // Un-hide the member notification
                mNotifications.first()->setHidden(false);
            }
        }

        emit changed();
        return true;
    }

    return false;
}

void NotificationGroup::onNotificationChanged()
{
    PersonalNotification *pn = qobject_cast<PersonalNotification*>(sender());
    if (!pn || !mNotifications.contains(pn))
        return;

    if (pn->hasPendingEvents())
        emit changed();
}

void NotificationGroup::onNotificationClosed(PersonalNotification *notification)
{
    removeNotification(notification);
}

void NotificationGroup::onClosed(uint /*reason*/)
{
    removeGroup();
}

