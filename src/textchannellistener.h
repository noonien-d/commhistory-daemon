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

#ifndef TEXT_CHANNEL_LISTENER_H
#define TEXT_CHANNEL_LISTENER_H

#include <QList>
#include <QMultiHash>

#include <CommHistory/Group>

#include "channellistener.h"
#include "constants.h"

namespace CommHistory {
    class GroupModel;
    class Event;
    class SingleEventModel;
    class ConversationModel;
}

namespace Tp {
    class Message;
    class ReceivedMessage;
    class PendingOperation;
    class Presence;

    namespace Client {
        class PropertiesInterfaceInterface;
    }
}

namespace RTComLogger
{

/*!
 * \class TextChannelListener
 * \brief class responsible for listening and logging activity on a text channel
 * chats, sms
 */
class TextChannelListener : public ChannelListener
{
    Q_OBJECT

public:

    TextChannelListener(const Tp::AccountPtr &account,
                        const Tp::ChannelPtr &channel,
                        const Tp::MethodInvocationContextPtr<> &context,
                        QObject *parent = 0);

    virtual ~TextChannelListener();

Q_SIGNALS:
    /*!
     * \brief emitted when message saving fails
     */
    void savingFailed(const Tp::ConnectionPtr& connection);

private Q_SLOTS:
    void slotMessageReceived(const Tp::ReceivedMessage &message);
    void slotMessageSent(const Tp::Message &message,
                       Tp::MessageSendingFlags flags,
                       const QString &messageToken);
    void slotOnModelReady(bool status);
    void slotPresenceChanged(const Tp::Presence &presence);
    void slotGroupRemoved(const QModelIndex &index, int start, int end);
    void slotGroupInserted(const QModelIndex &index, int start, int end);
    void slotGroupDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void slotEventsCommitted(QList<CommHistory::Event> events, bool status);
    void slotContactsReady(Tp::PendingOperation* operation);
    void slotPropertiesChanged(const Tp::PropertyValueList &props, bool listProps = false);
    void slotGroupMembersChanged(const Tp::Contacts &groupMembersAdded,
                                 const Tp::Contacts &groupLocalPendingMembersAdded,
                                 const Tp::Contacts &groupRemotePendingMembersAdded,
                                 const Tp::Contacts &groupMembersRemoved,
                                 const Tp::Channel::GroupMemberChangeDetails &details);
    void slotHandleOwnersFetched(Tp::PendingOperation *op);
    void slotHandleOwnersChanged(const Tp::HandleOwnerMap &handleOwnerMap,
                                 const Tp::UIntList &added,
                                 const Tp::UIntList &removed);
    void slotListPropertiesFinished(QDBusPendingCallWatcher *watcher);
    void slotGetPropertiesFinished(QDBusPendingCallWatcher *watcher);
    void slotExpungeMessages();
    void slotSaveFailedEvents();
    void slotJoinedGroupChat(Tp::PendingOperation *operation);
    void slotPendingMessageRemoved(const Tp::ReceivedMessage &message);
    void slotConvModelReady(bool success);
    void slotConvEventsCommitted(const QList<CommHistory::Event> &events, bool success);

private:

    enum ChangedChannelProperty {
        None,
        ChannelName,
        ChannelSubject
    };

    enum DeliveryHandlingStatus {
        DeliveryHandlingFailed,
        DeliveryHandlingResolved,
        DeliveryHandlingPending
    };

    void channelReady();
    void channelListenerReady();
    void requestConversationId();
    int groupId();
    void handleTpProperties();

    // delivery report
    DeliveryHandlingStatus handleDeliveryReport(const Tp::ReceivedMessage &message,
                                                CommHistory::Event &event);
    // MMS
    // normal message
    void fillEventFromMessage(const Tp::Message &message, CommHistory::Event &event);
    void handleReceivedMessage(const Tp::ReceivedMessage &message,
                               CommHistory::Event &event);

    void handleMessages();
    bool checkStoredMessagesIf();
    void expungeMessage(const QString &token);
    void updateGroupChatName(ChangedChannelProperty changedChannelProperty,
                             bool suppressGroupChatEvents);

    void fetchContacts();
    void handleMessageFailed(const Tp::ReceivedMessage &message,
                             const CommHistory::Event &event);
    void sendGroupChatEvent(const QString &message);
    void showErrorNote(const QString &errorMsg, const QString &category = ErrorCategory);
    void updateCurrentGroup(int start, int end, const QModelIndex &parent = QModelIndex());

    // attempt to read original message from delivery report
    bool recoverDeliveryEcho(const Tp::Message &message, CommHistory::Event &event);

    CommHistory::Event::EventType eventType() const;
    bool getEventForToken(const QString &token, const QString &mmsId,
                          int groupId, CommHistory::Event &event);
    bool getEventById(int eventId, CommHistory::Event &event);

    void saveMessage(CommHistory::Event &event);
    virtual void finishedWithError(const QString& errorName, const QString& errorMessage);

    bool hasPendingOperations() const;
    void tryToClose();
    void closed();

    CommHistory::Group getGroupById(int groupId) const;

    bool pendingCommit(const QString &messageToken);

    bool areRemotePartiesOffline();

    CommHistory::ConversationModel& conversationModel();

private:

    typedef QPair<QString,QString> Presence;

    // TODO: only for 1-1 chat, should be fixed later
    Tp::ContactPtr m_TargetContact;

    CommHistory::GroupModel *m_GroupModel;
    CommHistory::Group m_Group;
    bool m_GroupRequested;

    // tokens for expunging
    QList<QString> m_expungeTokens;
    // map event id to tokens that should be expunged,
    // Event does not have report delivery token, therefore it's stored here
    // until events are committed than if OK they are moved to m_expungeTokens
    // for actual expunging
    QMultiHash<int, QString> m_EventTokens;

    bool m_ShowOfflineChatError;

    // indicates that channel contains class0 messages
    bool m_isClassZeroSMS;

    Tp::HandleIdentifierMap m_HandleOwnerNames;
    Tp::Client::PropertiesInterfaceInterface *m_PropertiesIf;
    QHash<QString, Tp::PropertySpec> m_Properties;
    QHash<QString,Presence> m_PresenceStatuses;

    bool m_IsGroupChat;
    uint m_GroupHandleType;
    QString m_GroupChatName;
    QString m_ChannelName;
    QString m_ChannelSubject;
    uint m_ChannelSubjectContactHandle;
    QString m_PersistentId;

    // internal copy of message queue
    QList<Tp::ReceivedMessage> m_messageQueue;
    // flag to destroy listener as soon as all pending operations (updating events, expunging) complete
    bool m_channelClosed;
    // groups that have added events but have not yet emitted updated signal
    QList<int> m_pendingGroups;
    // added events but not committed yet, delivery report will
    // not be handled unitl the event committed
    QSet<QString> m_commitingEvents;

    //handle failed save messages
    uint m_FailedSaveCount;
    QList<CommHistory::Event> m_failedSaveEvents;

    // Global storage, among all text channel listeners, of ids of messages not acknowledged yet by mui:
    static QMultiHash<QString,uint> m_pendingMessageIds;

    QList<Tp::ReceivedMessage> m_replaceMessages;
    QList<CommHistory::Event> m_replaceEvents;
    CommHistory::ConversationModel* m_pConversationModel;
#ifdef UNIT_TEST
    friend class Ut_TextChannelListener;
#endif
};

} // namespave RTComLogger

#endif // CHANNEL_LISTENER_H
