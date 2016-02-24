/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013-2015 Jolla Ltd.
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

#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

// QT includes
#include <QObject>
#include <QHash>
#include <QPair>
#include <QDBusInterface>
#include <QFile>
#include <QQueue>
#include <QMultiHash>
#include <QModelIndex>

#include <qofonomessagewaiting.h>

#include <CommHistory/Event>
#include <CommHistory/Group>
#include <CommHistory/GroupModel>
#include <CommHistory/ContactListener>
#include <CommHistory/ContactResolver>

// our includes
#include "commhistoryservice.h"
#include "notificationgroup.h"
#include "personalnotification.h"

namespace Ngf {
    class Client;
}

namespace RTComLogger {

typedef QPair<QString,QString> TpContactUid;

/*!
 * \class NotificationManager
 * \brief class responsible for showing notifications on desktop
 */
class NotificationManager : public QObject
{
    Q_OBJECT

public:
    typedef CommHistory::RecipientList RecipientList;

    struct EventGroupProperties {
        PersonalNotification::EventCollection collection;
        QString localUid;
        QString remoteUid;

        bool operator== (const EventGroupProperties &other) const { return (collection == other.collection && localUid == other.localUid && remoteUid == other.remoteUid); }
    };

    static EventGroupProperties eventGroup(PersonalNotification::EventCollection c, const QString &l, const QString &r) {
        EventGroupProperties properties;
        properties.collection = c;
        properties.localUid = l;
        properties.remoteUid = r;
        return properties;
    }

    /*!
     *  \param QObject parent object
     *  \returns Notification manager singleton
     */
    static NotificationManager* instance();

    /*!
     * \brief shows notification
     * \param event to be shown
     */
    void showNotification(const CommHistory::Event& event,
                          const QString &channelTargetId = QString(),
                          CommHistory::Group::ChatType chatType = CommHistory::Group::ChatTypeP2P,
                          const QString &details = QString());

    /*!
     * \brief removes notifications whose event type is in the supplied list of types
     */
    void removeNotificationTypes(const QList<int> &types);

    /*!
     * \brief return group model with all conversations
     * \returns group model pointer
     */
    CommHistory::GroupModel* groupModel();

    /*!
     * \brief Show voicemail notification or removes it if count is 0
     * \param count number of voicemails if it's known,
     *              a negative number if the number is unknown
     */
    void showVoicemailNotification(int count);

    /*!
     * \brief Play class 0 SMS alert
     */
    void playClass0SMSAlert();
    void requestClass0Notification(const CommHistory::Event &event);

    void setNotificationProperties(Notification *notification, PersonalNotification *pn, bool grouped);

public Q_SLOTS:
    /*!
     * \brief Removes notifications belonging to a particular account having optionally certain remote uids.
     * \param accountPath Notifications of this account are to be removed.
     */
    void removeNotifications(const QString &accountPath, const QList<int> &removeTypes = QList<int>());

private Q_SLOTS:
    /*!
     * Initialises notification manager instance
     */
    void init();
    void slotObservedConversationsChanged(const QList<CommHistoryService::Conversation> &conversations);
    void slotInboxObservedChanged();
    void slotCallHistoryObservedChanged(bool observed);
    void slotGroupRemoved(const QModelIndex &index, int start, int end);
    void slotGroupDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void slotNgfEventFinished(quint32 id);
    void slotContactResolveFinished();
    void slotContactChanged(const RecipientList &recipients);
    void slotContactInfoChanged(const RecipientList &recipients);
    void slotClassZeroError(const QDBusError &error);
    void slotVoicemailWaitingChanged();
    void slotModemAdded(QString path);
    void slotModemRemoved(QString path);
    void slotModemsChanged(QStringList modems);
    void slotValidChanged(bool valid);

private:
    NotificationManager( QObject* parent = 0);
    ~NotificationManager();
    bool isCurrentlyObservedByUI(const CommHistory::Event& event,
                                 const QString &channelTargetId,
                                 CommHistory::Group::ChatType chatType);

    void resolveNotification(PersonalNotification *notification);
    void addNotification(PersonalNotification *notification);

    void removeConversationNotifications(const CommHistory::Recipient &recipient,
                                         CommHistory::Group::ChatType chatType);

    void syncNotifications();
    int pendingEventCount();
    void clearPendingEvents(const NotificationGroup &group);
    void removeNotPendingEvents(const NotificationGroup &group);

    bool isFilteredInbox();
    QString filteredInboxAccountPath();
    bool updateEditedEvent(const CommHistory::Event &event, const QString &text);
    void addModem(QString path);

    QString notificationText(const CommHistory::Event &event, const QString &details);

private:
    static NotificationManager* m_pInstance;
    QHash<EventGroupProperties, NotificationGroup*> m_Groups;
    bool m_Initialised;

    QList<PersonalNotification*> m_unresolvedNotifications;

    CommHistory::ContactResolver *m_contactResolver;
    QSharedPointer<CommHistory::ContactListener> m_contactListener;
    CommHistory::GroupModel *m_GroupModel;

    Ngf::Client *m_ngfClient;
    quint32 m_ngfEvent;

    QHash<QString,QOfonoMessageWaiting*> interfaces;

#ifdef UNIT_TEST
    friend class Ut_NotificationManager;
#endif
};

inline uint qHash(const NotificationManager::EventGroupProperties &properties, uint seed)
{
    using ::qHash;
    return qHash(properties.collection, seed) ^ qHash(properties.localUid, seed) ^ qHash(properties.remoteUid, seed);
}

} // namespace RTComLogger

#endif // NOTIFICATIONMANAGER_H
