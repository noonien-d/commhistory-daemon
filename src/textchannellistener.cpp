/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2014-2015 Jolla Ltd.
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

// QT
#include <QtDBus/QtDBus>

// MeegoTouch
#include <MLocale>

// Nemomobile
#include <notification.h>

// libcommhistory
#include <CommHistory/EventModel>
#include <CommHistory/GroupModel>
#include <CommHistory/Event>
#include <CommHistory/Group>
#include <CommHistory/commonutils.h>
#include <CommHistory/SingleEventModel>
#include <CommHistory/DatabaseIO>
#include <CommHistory/ConversationModel>

// Telepathy
#include <TelepathyQt/TextChannel>
#include <TelepathyQt/Message>
#include <TelepathyQt/Contact>
#include <TelepathyQt/ContactManager>
#include <TelepathyQt/PendingContacts>
#include <TelepathyQt/Connection>
#include <TelepathyQt/Properties>
#include <TelepathyQt/Presence>
#include <TelepathyQt/Constants>
#include <TelepathyQt/types.h>

#include <TpExtensions/Connection> // stored messages if

// Contacts
#include <QContact>
#include <QContactManager>
#include <QVersitReader>
#include <QVersitContactImporter>

#include "textchannellistener.h"
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"
#include "debug.h"

// LOCAL DEFINITIONS
// delivery report
#define DELIVERY_STATUS       QLatin1String("delivery-status")
#define DELIVERY_TOKEN        QLatin1String("delivery-token")
#define DELIVERY_TO           QLatin1String("delivery-to")
#define DELIVERY_DBUSERROR    QLatin1String("delivery-dbus-error")
#define DELIVERY_ERRORMESSAGE QLatin1String("delivery-error-message")
#define DELIVERY_ECHO         QLatin1String("delivery-echo")

// errors
#define MODEM_ERROR_SMSC_ADDRESS_NOT_AVAILABLE         "com.nokia.Modem.SMS.Errors.SMSCAddressNotAvailable"
#define MODEM_ERROR_DESTINATION_ADDRESS_FDN_RESTRICTED "com.nokia.Modem.SMS.Errors.DestinationAddressFDNRestricted"
#define MODEM_ERROR_SMS_ADDRESS_FDN_RESTRICTED         "com.nokia.Modem.SMS.Errors.SMSCAddressFDNRestricted"

// voicemail
#define TXT_VOICE            QLatin1String("voice")
#define MAILBOX_NOTIFICATION QLatin1String("x-nokia-mailbox-notification")
#define VOICEMAIL_TYPE       QLatin1String("x-nokia-voicemail-type")
#define MAILBOX_UNREAD_COUNT QLatin1String("x-nokia-mailbox-unread-count")
#define MAILBOX_HAS_UNREAD   QLatin1String("x-nokia-mailbox-has-unread")

// content
#define PART_CONTENT       QLatin1String("content")
#define PART_CONTENT_TYPE  QLatin1String("content-type")
#define TXT_CONTENT_TYPE   QLatin1String("text/plain")

// message editing support
#define SUPERSEDES_TOKEN    QLatin1String("supersedes")

#define PROTOCOL_TEL QLatin1String("tel")

// tp properties
#define CHANNEL_PROPERTY_NAME QLatin1String("name")
#define CHANNEL_PROPERTY_SUBJECT QLatin1String("subject")
#define CHANNEL_PROPERTY_SUBJECT_CONTACT QLatin1String("subject-contact")

#define PENDING_MESSAGE_ID_PROPERTY_NAME QLatin1String("pending-message-id")

#define SUBSCRIBER_ID_PROPERTY_NAME ("SubscriberIdentity")
#define SUBSCRIBER_IDENTITY_HEADER_KEY ("subscriber-identity")

#define MAX_SAVE_ATTEMPTS 3
#define RESAVE_INTERVAL 5000 //ms

using namespace RTComLogger;
using namespace CommHistory;
QTCONTACTS_USE_NAMESPACE
QTVERSIT_USE_NAMESPACE

namespace {

CommHistory::Event::PropertySet deliveryHandlingProperties = CommHistory::Event::PropertySet()
                                                 << CommHistory::Event::Id
                                                 << CommHistory::Event::Type
                                                 << CommHistory::Event::StartTime
                                                 << CommHistory::Event::EndTime
                                                 << CommHistory::Event::Direction
                                                 << CommHistory::Event::IsRead
                                                 << CommHistory::Event::Status
                                                 << CommHistory::Event::LocalUid
                                                 << CommHistory::Event::RemoteUid
                                                 << CommHistory::Event::GroupId
                                                 << CommHistory::Event::MessageToken
                                                 << CommHistory::Event::Subject
                                                 << CommHistory::Event::FreeText
                                                 << CommHistory::Event::FromVCardFileName
                                                 << CommHistory::Event::FromVCardLabel
                                                 << CommHistory::Event::ContentLocation
                                                 << CommHistory::Event::MessageParts
                                                 << CommHistory::Event::ReportDelivery
                                                 << CommHistory::Event::ReadStatus
                                                 << CommHistory::Event::ReportReadRequested
                                                 << CommHistory::Event::ReportRead;


template<typename T>
T partValue(const Tp::MessagePart &part, const QString &key, const T &defaultValue = T())
{
    if (part.contains(key)) {
        const QVariant var(part.value(key).variant());
        if (var.isValid())
            return var.value<T>();
    }

    return defaultValue;
}

bool isVoicemail(const Tp::MessagePart &header)
{
    return (partValue<QString>(header, MAILBOX_NOTIFICATION) == TXT_VOICE);
}

QString replaceType(const Tp::MessagePart &header)
{
    return partValue<QString>(header, REPLACE_TYPE);
}

QString voicemailType(const Tp::MessagePart &header)
{
    // check whether it's skype or sms voicemail, aka
    // "x-nokia-voicemail-type" is "skype" or "tel"
    // Both SMS and Skype voicemail notifications (x-nokia-mailbox-notification
    // with value "voice") MUST have this header
    return partValue<QString>(header, VOICEMAIL_TYPE);
}

bool mailboxHasUnread(const Tp::MessagePart &header)
{
    return partValue<bool>(header, MAILBOX_HAS_UNREAD, false);
}

uint mailboxUnreadCount(const Tp::MessagePart &header)
{
    return partValue<uint>(header, MAILBOX_UNREAD_COUNT, 0u);
}

QString supersedesToken(const Tp::MessagePart &header)
{
    return partValue<QString>(header, SUPERSEDES_TOKEN);
}

uint pendingId(const Tp::ReceivedMessage &message)
{
    return partValue<uint>(message.header(), PENDING_MESSAGE_ID_PROPERTY_NAME, 0u);
}

QString subscriberIdentity(const Tp::MessagePart &header)
{
    return partValue<QString>(header, SUBSCRIBER_IDENTITY_HEADER_KEY);
}

} // anonymous namespace

QMultiHash<QString,uint> TextChannelListener::m_pendingMessageIds;

TextChannelListener::TextChannelListener(const Tp::AccountPtr &account,
                                         const Tp::ChannelPtr &channel,
                                         const Tp::MethodInvocationContextPtr<> &context,
                                         QObject *parent)
    : ChannelListener(account, channel, context, parent),
      m_GroupModel(0),
      m_GroupRequested(false),
      m_ShowOfflineChatError(true),
      m_isClassZeroSMS(false),
      m_PropertiesIf(0),
      m_IsGroupChat(false),
      m_channelClosed(false),
      m_FailedSaveCount(0),
      m_pConversationModel(0)
{
    DEBUG() << __PRETTY_FUNCTION__;
    makeChannelReady(Tp::TextChannel::FeatureMessageQueue
                     | Tp::TextChannel::FeatureMessageSentSignal);
}

void TextChannelListener::channelListenerReady()
{
    DEBUG() << __PRETTY_FUNCTION__;

    if (m_IsGroupChat)
        handleTpProperties();

    Tp::TextChannelPtr textChannel = Tp::TextChannelPtr::dynamicCast(m_Channel);

    if(!textChannel.isNull()) {
        connect( textChannel.data(),
                 SIGNAL( messageReceived(const Tp::ReceivedMessage&) ),
                 SLOT( slotMessageReceived(const Tp::ReceivedMessage&) ),
                 Qt::UniqueConnection);

        // When message in text channel is acknowledged, the pendingMessageRemoved signal is emitted:
        connect( textChannel.data(),
                 SIGNAL( pendingMessageRemoved(const Tp::ReceivedMessage&) ),
                 SLOT( slotPendingMessageRemoved(const Tp::ReceivedMessage&) ),
                 Qt::UniqueConnection);

        connect( textChannel.data(),
                 SIGNAL( messageSent(const Tp::Message&, Tp::MessageSendingFlags, const QString&) ),
                 SLOT( slotMessageSent(const Tp::Message&, Tp::MessageSendingFlags, const QString&) ),
                 Qt::UniqueConnection );

        // check if channel is meant to be used for class 0 sms messages
        QVariantMap properties = textChannel->immutableProperties();

        QVariant property = properties.value(TP_QT_IFACE_CHANNEL_INTERFACE_SMS + QLatin1String(".Flash"), QVariant());
        if(property.isValid() && property.value<bool>() == true) {
            DEBUG() << __FUNCTION__ << "Channel contains class 0 property";
            m_isClassZeroSMS = true;
        }

        handleMessages();
    } else {
        qCritical() << Q_FUNC_INFO << "Wrong channel - Null";
    }
    invocationContextFinished();
}

bool TextChannelListener::checkStoredMessagesIf()
{
    if (m_Connection.isNull() || !m_Connection->isReady()) {
        qCritical() << Q_FUNC_INFO << "Connection is not ready when it should be";
        return false;
    }

    return m_Connection->hasInterface(CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface::staticInterfaceName());
}

void TextChannelListener::requestConversationId()
{
    if (!m_GroupRequested) {
        m_GroupModel = NotificationManager::instance()->groupModel();
        if (m_GroupModel) {
            m_GroupRequested = true;

            connect(m_GroupModel, SIGNAL(rowsAboutToBeRemoved(const QModelIndex&,int,int)),
                    SLOT(slotGroupRemoved(const QModelIndex&,int,int)));
            connect(m_GroupModel, SIGNAL(dataChanged(const QModelIndex&,const QModelIndex&)),
                    SLOT(slotGroupDataChanged(const QModelIndex&,const QModelIndex&)));
            connect(m_GroupModel, SIGNAL(rowsInserted(const QModelIndex &, int, int)),
                    this, SLOT(slotGroupInserted(const QModelIndex &, int, int)));

            if (m_GroupModel->isReady()) {
                slotOnModelReady(true);
            } else {
                connect(m_GroupModel, SIGNAL(modelReady(bool)), SLOT(slotOnModelReady(bool)));
            }
        } else {
            qCritical() << "Failed to create group model";
        }
    }
}

TextChannelListener::~TextChannelListener()
{
}

void TextChannelListener::slotGroupDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    if (!m_Group.isValid())
        return;

    if (!topLeft.isValid() || !bottomRight.isValid()) {
        qWarning() << Q_FUNC_INFO << "Invalid indexes";
        return;
    }

    bool pendingGroupsHandled = false;

    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
        QModelIndex row = m_GroupModel->index(i, 0);
        CommHistory::Group group = m_GroupModel->group(row);
        if (group.isValid()) {
            if (m_pendingGroups.contains(group.id())) {
                pendingGroupsHandled = true;
                m_pendingGroups.removeAll(group.id());
            }

            if (m_Group.id() == group.id())
                m_Group = group;
        }
    }

    if (pendingGroupsHandled)
        handleMessages();

    tryToClose();
}

void TextChannelListener::slotGroupInserted(const QModelIndex &index, int start, int end)
{
    DEBUG() << Q_FUNC_INFO << "Account path handled by this listener: " << m_Account->objectPath();
    DEBUG() << Q_FUNC_INFO << "Target handled by this listener: " << targetId();

    updateCurrentGroup(start, end, index);
}

void TextChannelListener::slotGroupRemoved(const QModelIndex &index, int start, int end)
{
    DEBUG() << Q_FUNC_INFO << "Account path handled by this listener: " << m_Account->objectPath();
    DEBUG() << Q_FUNC_INFO << "Target handled by this listener: " << targetId();

    if (!m_Group.isValid())
    {
        DEBUG() << Q_FUNC_INFO << "Group is not valid!";
        return;
    }

    for (int i = start; i <= end; i++) {
        QModelIndex row = m_GroupModel->index(i, 0, index);
        CommHistory::Group group = m_GroupModel->group(row);
        if (group == m_Group) {
            DEBUG() << Q_FUNC_INFO << "Removed group belongs to this listener!";
            m_Group.setId(-1); // Invalidate the current group in this listener.
            break;
        }
    }
}

int TextChannelListener::groupId()
{
    DEBUG() << Q_FUNC_INFO;
    if (!m_Group.isValid()) {
        DEBUG() << Q_FUNC_INFO << "Group is not valid!";

        if (m_GroupModel->isReady()
            && m_Account) { // m_Account not need to be ready

            CommHistory::Group group;
            group.setLocalUid(m_Account->objectPath());

            DEBUG() << Q_FUNC_INFO << targetId();
            group.setRecipients(Recipient(m_Account->objectPath(), targetId()));

            if (m_IsGroupChat) {
                CommHistory::Group::ChatType chatType = CommHistory::Group::ChatTypeP2P;
                if (m_GroupHandleType == Tp::HandleTypeNone) {
                    chatType = CommHistory::Group::ChatTypeUnnamed;
                } else if (m_GroupHandleType == Tp::HandleTypeRoom) {
                    chatType = CommHistory::Group::ChatTypeRoom;
                }
                group.setChatType(chatType);

                if (!m_GroupChatName.isEmpty())
                    group.setChatName(m_GroupChatName);
            }

            if (!m_GroupModel->addGroup(group)) {

                qCritical() << Q_FUNC_INFO << "error adding group";
            }
            else {

                m_Group = group;
                DEBUG() << Q_FUNC_INFO << "added new group:" << m_Group.id();
            }
        }
        else {

            qWarning() << "Group id is requested in invalid state";
        }
    }

    return m_Group.id();
}

void TextChannelListener::handleTpProperties()
{
    m_PropertiesIf = new Tp::Client::PropertiesInterfaceInterface(
        *(m_Channel->interface<Tp::Client::ChannelInterface>()));
    connect(m_PropertiesIf, SIGNAL(PropertiesChanged(const Tp::PropertyValueList &)),
            this, SLOT(slotPropertiesChanged(const Tp::PropertyValueList &)));
    QDBusPendingCall propertyCall = m_PropertiesIf->ListProperties();
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(propertyCall, this);
    connect(watcher, SIGNAL(finished(QDBusPendingCallWatcher *)),
            this, SLOT(slotListPropertiesFinished(QDBusPendingCallWatcher *)));
}

void TextChannelListener::slotListPropertiesFinished(QDBusPendingCallWatcher *watcher)
{
    DEBUG() << Q_FUNC_INFO;

    QDBusPendingReply<Tp::PropertySpecList> reply = *watcher;
    if (!reply.isValid()) {
        qWarning() << Q_FUNC_INFO << "ListProperties failed:" << reply.error();
        watcher->deleteLater();
        return;
    }

    foreach (Tp::PropertySpec spec, reply.value())
        m_Properties.insert(spec.name, spec);

    Tp::UIntList propIds;
    if (m_Properties.contains(CHANNEL_PROPERTY_NAME)
        && (m_Properties.value(CHANNEL_PROPERTY_NAME).flags & Tp::PropertyFlagRead))
        propIds << m_Properties.value(CHANNEL_PROPERTY_NAME).propertyID;

    if (m_Properties.contains(CHANNEL_PROPERTY_SUBJECT)
        && (m_Properties.value(CHANNEL_PROPERTY_SUBJECT).flags & Tp::PropertyFlagRead))
        propIds << m_Properties.value(CHANNEL_PROPERTY_SUBJECT).propertyID;

    if (m_Properties.contains(CHANNEL_PROPERTY_SUBJECT_CONTACT)
        && (m_Properties.value(CHANNEL_PROPERTY_SUBJECT_CONTACT).flags & Tp::PropertyFlagRead))
        propIds << m_Properties.value(CHANNEL_PROPERTY_SUBJECT_CONTACT).propertyID;

    DEBUG() << Q_FUNC_INFO << propIds;

    if (!propIds.isEmpty()) {
        QDBusPendingCall getPropertyCall = m_PropertiesIf->GetProperties(propIds);
        QDBusPendingCallWatcher *getWatcher = new QDBusPendingCallWatcher(getPropertyCall, this);
        connect(getWatcher, SIGNAL(finished(QDBusPendingCallWatcher *)),
                this, SLOT(slotGetPropertiesFinished(QDBusPendingCallWatcher *)));
    }

    watcher->deleteLater();
}

void TextChannelListener::slotGetPropertiesFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<Tp::PropertyValueList> reply = *watcher;
    if (!reply.isValid()) {
        DEBUG() << Q_FUNC_INFO << "GetProperties failed:" << reply.error();
        watcher->deleteLater();
        return;
    }
    slotPropertiesChanged(reply.value(), true);
    watcher->deleteLater();
}

void TextChannelListener::slotOnModelReady(bool status)
{
    DEBUG() << __PRETTY_FUNCTION__ << m_Account->objectPath() << targetId();

    disconnect(m_GroupModel, SIGNAL(modelReady(bool)),
               this, SLOT(slotOnModelReady(bool)));

    if (!status) {
        qCritical() << "Group model failed to load";
        return;
    }

    // if group exist, read group id right away
    // otherwise add a new group only when a new message(received/sent) comes
    const int groupCount = m_GroupModel->rowCount();
    if (groupCount > 0 && m_Account) {
        updateCurrentGroup(0, groupCount - 1);
    }

    channelListenerReady();
}

void TextChannelListener::slotMessageReceived(const Tp::ReceivedMessage &message)
{
    Q_UNUSED(message);
    DEBUG() << __PRETTY_FUNCTION__;

    handleMessages();
}

void TextChannelListener::slotPendingMessageRemoved(const Tp::ReceivedMessage &message)
{
    DEBUG() << __PRETTY_FUNCTION__;

    uint id = pendingId(message);

    DEBUG() << __PRETTY_FUNCTION__ << "Pending message (pending id = " << id << ") having content "
             << message.text() << " acked and removed from channel's message queue.";

    if (m_pendingMessageIds.remove(m_Channel->objectPath(), id) > 0) {
        DEBUG() << __PRETTY_FUNCTION__ << "Removing message from channel " << m_Channel->objectPath()
                 << " having pending id " << id << " from pending messages list of all text channel listeners";
    }
}

void TextChannelListener::handleMessages()
{
    QList<CommHistory::Event> scrollbackEvents;
    QList<CommHistory::Event> addEvents;
    QHash<int, QList<CommHistory::Event> > modifyEvents; // separate list for each group
    QList<Tp::ReceivedMessage> processedMessages;
    QList<Tp::ReceivedMessage> addMessages;
    QHash<int, QList<Tp::ReceivedMessage> > modifyMessages;
    // expunge tokens for committing events
    QHash<int, QMultiHash<int, QString> > modifyTokens;
    bool hasReplaceMessage = false;

    NotificationManager* nManager = NotificationManager::instance();

    Tp::TextChannelPtr textChannel = Tp::TextChannelPtr::dynamicCast(m_Channel);
    if (!textChannel) {
        qCritical() << "TextChannelListener has non text channel";
        return;
    }

    // Add to our local message queue only those messages that are not yet pending:
    foreach (Tp::ReceivedMessage me, textChannel->messageQueue()) {
        uint id = pendingId(me);
        if (!m_pendingMessageIds.contains(m_Channel->objectPath(), id)) {
            m_messageQueue << me;
            m_pendingMessageIds.insertMulti(m_Channel->objectPath(), id);
        }
    }

    DEBUG() << __PRETTY_FUNCTION__ << "Number of messages in local message queue: " << m_messageQueue.size();

    foreach(Tp::ReceivedMessage message, m_messageQueue) {
        CommHistory::Event event;
        Tp::ChannelTextMessageType type = message.messageType();
        bool wait = false;

        DEBUG() << __PRETTY_FUNCTION__ << "Handling message from channel " << m_Channel->objectPath()
                 << " with content " << message.text() << " and with pending id " << pendingId(message);

        switch (type) {
        case Tp::ChannelTextMessageTypeDeliveryReport: {
            DeliveryHandlingStatus status = handleDeliveryReport(message, event);
            switch (status) {
            case DeliveryHandlingResolved:
                if (m_pendingGroups.contains(event.groupId())) {
                    wait = true;
                    break;
                }

                if (event.isValid()) {
                    int groupId = event.groupId();
                    modifyEvents[groupId] << event;
                    modifyMessages[groupId] << message;

                    QString token = message.messageToken();
                    if (token.isEmpty())
                        token = event.messageToken();

                    modifyTokens[groupId].insertMulti(event.id(), token);

                } else {
                    DEBUG() << __PRETTY_FUNCTION__ << "Ignoring recovered message from delivery echo";
                }

                break;
            case DeliveryHandlingFailed:
                expungeMessage(message.messageToken());
                processedMessages << message;
                break;
            case DeliveryHandlingPending:
                wait = true;
                break;
            default:
                qCritical() << "Unknown DeliveryHandlingStatus" << status;
            }
            break;
        }
        case Tp::ChannelTextMessageTypeNormal: {
            // fills event properties
            handleReceivedMessage(message, event);

            QString replaceTypeValue = replaceType(message.header());

            // class 0 sms
            if (m_isClassZeroSMS) {
                DEBUG() << __FUNCTION__ << "Handling class 0 sms";
                processedMessages << message;
                nManager->playClass0SMSAlert();
                nManager->requestClass0Notification(event);
                expungeMessage(event.messageToken());
            // Replace sms
            } else if (!replaceTypeValue.isEmpty()) {
                DEBUG() << __FUNCTION__ << "Replace type of sms";
                m_replaceEvents << event;
                m_replaceMessages << message;
                hasReplaceMessage = true;
                if (event.direction() != CommHistory::Event::Outbound) {
                    nManager->showNotification(event, targetId(), m_Group.chatType());
                }
              // Normal sms
            } else {
                QString supersedes = supersedesToken(message.header());
                bool silent = message.isSilent();

                if (!supersedes.isEmpty()) {
                    CommHistory::Event originalEvent;
                    getEventForToken(supersedes, QString(), m_Group.id(), originalEvent);
                    if (!originalEvent.isValid()) {
                        // handle as a new message
                        // use original's message token to be able to handle updates
                        // for this message
                        event.setMessageToken(supersedes);
                        addEvents << event;
                        addMessages << message;
                        if (!silent) {
                            nManager->showNotification(event, targetId(), m_Group.chatType());
                        }
                    } else {
                        //update message
                        originalEvent.setFreeText(event.freeText());
                        originalEvent.setStartTime(event.startTime());
                        if (originalEvent.isRead())
                            originalEvent.setIsRead(false);

                        modifyEvents[event.groupId()] << originalEvent;
                        modifyMessages[event.groupId()] << message;
                        modifyTokens[event.groupId()].insertMulti(originalEvent.id(),originalEvent.messageToken());

                        if (!silent) {
                            nManager->showNotification(originalEvent, targetId(), m_Group.chatType());
                        }
                    }
                } else {
                    if (message.isScrollback()) {
                        scrollbackEvents << event;
                    } else {
                        addEvents << event;
                    }
                    addMessages << message;

                    if (event.direction() != CommHistory::Event::Outbound) {
                        if (!silent) {
                            nManager->showNotification(event, targetId(), m_Group.chatType());
                        }
                    }
                }
            }
            break;
        }
        case Tp::ChannelTextMessageTypeNotice: {
            // Telepathy can add a new type for mailbox notification in future
            Tp::MessagePart header = message.header();
            if (isVoicemail(header)) {
                QString type = voicemailType(header);

                if (type == PROTOCOL_TEL) {
                    int unread = 0;

                    if (mailboxHasUnread(header)) {
                        unread = mailboxUnreadCount(message.header());
                        if (unread == 0)
                            unread = -1; // set count to -1 if unread flag is set
                                         // but count is not available
                                         // as requiried by USIM MWI notation
                    }

                    nManager->showVoicemailNotification(unread);
                }
                // TODO skype voicemail support
            }
            expungeMessage(message.messageToken());
            processedMessages << message;
            break;
        }
        case Tp::ChannelTextMessageTypeAction: {
            handleReceivedMessage(message, event);
            event.setIsAction(true);

            if (message.isScrollback()) {
                scrollbackEvents << event;
            } else {
                addEvents << event;
            }
            addMessages << message;

            if (event.direction() != CommHistory::Event::Outbound) {
                nManager->showNotification(event, targetId(), m_Group.chatType());
            }
        break;
        }
        default:
            DEBUG() << "onMessageReceived: type " << type << " not supported";
            break;
        }

        if (wait)
            break;
    }

    if (!scrollbackEvents.isEmpty()) {
        if (eventModel().addEvents(scrollbackEvents, true)) {
            processedMessages << addMessages;
        } else {
            qWarning() << "Adding events failed";
        }
    }

    if (!addEvents.isEmpty()) {
        if (eventModel().addEvents(addEvents)) {
            processedMessages << addMessages;
            foreach (CommHistory::Event e, addEvents)
                m_EventTokens.insertMulti(e.id(), e.messageToken());
        } else {
            qWarning() << "Adding events failed";
        }
    }

    if (hasReplaceMessage) {
        if (conversationModel().isReady())
            conversationModel().getEvents(m_Group.id());
    }

    if (!modifyEvents.isEmpty()) {
        QHash<int, QList<CommHistory::Event> >::iterator i;
        for (i = modifyEvents.begin(); i != modifyEvents.end(); ++i) {
            CommHistory::Group group = getGroupById(i.key());
            if (group.isValid() && eventModel().modifyEventsInGroup(i.value(), group)) {
                processedMessages << modifyMessages[i.key()];
                m_EventTokens += modifyTokens[i.key()];
            } else {
                qWarning() << "Modify events failed for group" << i.key();
            }
        }
    }

    foreach (Tp::ReceivedMessage message, processedMessages) {
        m_messageQueue.removeOne(message);
    }
}

CommHistory::ConversationModel& TextChannelListener::conversationModel()
{
    if(!m_pConversationModel){
        m_pConversationModel = new CommHistory::ConversationModel(this);
        // We are interested only in replace type that will be stored into Headers property:
        m_pConversationModel->setPropertyMask(CommHistory::Event::PropertySet()
                                              << CommHistory::Event::Headers);
        // We are not interested in contact changes, just replace type of a message:
        m_pConversationModel->setResolveContacts(EventModel::DoNotResolve);
        // Model should inform us when it is populated after calling getEvents:
        connect(m_pConversationModel, SIGNAL(modelReady(bool)),
                this, SLOT(slotConvModelReady(bool)));
    }

    return *m_pConversationModel;
}

void TextChannelListener::slotConvModelReady(bool success)
{
    DEBUG() << __FUNCTION__;

    if (success && !m_replaceEvents.isEmpty()) {
        CommHistory::Event event = m_replaceEvents.takeFirst();

        // Model should inform us when event that we add is committed into the database.
        connect(&conversationModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event> &, bool)),
                this, SLOT(slotConvEventsCommitted(const QList<CommHistory::Event> &, bool)));

        if (conversationModel().addEvent(event)) {
            // Add event token to list to indicate that the event is under committing process.
            m_EventTokens.insertMulti(event.id(), event.messageToken());
        } else  {
            qWarning() << "Adding replace type of event failed!";
        }

        m_messageQueue.removeOne(m_replaceMessages.takeFirst());
    }
}

void TextChannelListener::slotConvEventsCommitted(const QList<CommHistory::Event> &events, bool success)
{
    DEBUG() << __FUNCTION__;

    disconnect(&conversationModel(), SIGNAL(eventsCommitted(const QList<CommHistory::Event> &, bool)),
            this, SLOT(slotConvEventsCommitted(const QList<CommHistory::Event> &, bool)));

    if (success) {
        QList<CommHistory::Event> eventsToBeRemoved;

        foreach (CommHistory::Event committedEvent, events) {
            // This should never happen, but still in case to avoid unnecessary conversation model looping:
            if (committedEvent.headers().value(REPLACE_TYPE).isEmpty()) continue;

            QString replaceTypeValueCommitted = committedEvent.headers().value(REPLACE_TYPE);

            for (int i=0;i<conversationModel().rowCount(QModelIndex());i++) {
                QModelIndex eventIndex = conversationModel().index(i,0);
                if (eventIndex.isValid()) {
                    CommHistory::Event event = conversationModel().event(eventIndex);
                    // Set previous voicemail SMS having same replace type to be removed:
                    if ((event.headers().value(REPLACE_TYPE) == replaceTypeValueCommitted)
                        && committedEvent.id() != event.id())
                        eventsToBeRemoved.append(event);
                }
            }
        }

        foreach (CommHistory::Event event, eventsToBeRemoved)
            if (!conversationModel().deleteEvent(event)) qWarning() << "Removing replace type of event failed!";
    }

    // Call to handle msg expunging, event tokens list and failed cases:
    slotEventsCommitted(events,success);

    // Check if there are more replace type of messages in message queue to be handled:
    slotConvModelReady(true);
}

bool TextChannelListener::recoverDeliveryEcho(const Tp::Message &message,
                                              CommHistory::Event &event)
{
    bool result = false;
    QVariant echoVar = message.header().value(DELIVERY_ECHO).variant();

    if (echoVar.isValid()) {
        Tp::MessagePartList parts = qdbus_cast<Tp::MessagePartList>(echoVar);
        if (parts.size() > 1) { // first one is header
            QString contentType = parts[1].value(PART_CONTENT_TYPE).variant().toString();
            QString content = parts[1].value(PART_CONTENT).variant().toString();

            if (contentType == TXT_CONTENT_TYPE) {
                event.setFreeText(content.trimmed());
                result = true;
            }
        }
    }

    return result;
}

bool TextChannelListener::getEventForToken(const QString &token,
                                           const QString &mmsId,
                                           int groupId,
                                           CommHistory::Event &event)
{
    CommHistory::SingleEventModel model;
    model.setQueryMode(CommHistory::SingleEventModel::SyncQuery);
    model.setPropertyMask(deliveryHandlingProperties);

    if (model.getEventByTokens(token, mmsId, groupId)) {
        if (model.rowCount() > 0)
            event = model.event();
        return true;
    } else {
        qWarning() << "Failed query single event model";
        return false;
    }
}

bool TextChannelListener::getEventById(int eventId, CommHistory::Event &event)
{
    CommHistory::SingleEventModel model;
    model.setQueryMode(CommHistory::SingleEventModel::SyncQuery);

    if (model.getEventById(eventId)) {
        if (model.rowCount() > 0)
            event = model.event();
        return true;
    } else {
        qWarning() << "Failed query single event model";
        return false;
    }
}

TextChannelListener::DeliveryHandlingStatus TextChannelListener::handleDeliveryReport(const Tp::ReceivedMessage &message,
                                                                                      CommHistory::Event &event)
{
    DeliveryHandlingStatus result = DeliveryHandlingFailed;

    DEBUG() << "[DELIVERY] Handling delivery report";
    if (message.messageToken().isNull()) {
        qWarning() << "[DELIVERY] Trying to handler delivery report, while message token is empty";
        return result;
    }

    // if we find message with the same token, update its status
    Tp::MessagePart header = message.header();
    QVariant tokenVar = header.value(DELIVERY_TOKEN).variant();

    QString deliveryToken;
    if (tokenVar.isValid()) {
        deliveryToken = tokenVar.value<QString>();
    } else {
        qWarning() << "[DELIVERY] Cannot fetch delivery token";
    }

    DEBUG() << "[DELIVERY] Message token is: " << deliveryToken;

    if (pendingCommit(deliveryToken)) {
        DEBUG() << "[DELIVERY] Original message is not committed yet, wait for it";
        return DeliveryHandlingPending;
    }

    // get status
    QVariant status = header.value(DELIVERY_STATUS).variant();

    bool messageFound = false;
    if (!deliveryToken.isEmpty()) {
        if (!getEventForToken(deliveryToken, QString(), m_Group.id(), event))
            return DeliveryHandlingFailed;
        messageFound = event.isValid();
    }

    // echo recovery
    if (!messageFound) {
        result = DeliveryHandlingFailed;
        messageFound = recoverDeliveryEcho(message, event);
        if (messageFound) {
            event.setMessageToken(deliveryToken);
            event.setType(eventType());
            event.setLocalUid(m_Account->objectPath());
            if (!m_isClassZeroSMS) {
                event.setGroupId(groupId());
            }
            event.setIsRead(true);
            event.setDirection(CommHistory::Event::Outbound);
            event.setRecipients(Recipient(m_Account->objectPath(), targetId()));

            QDateTime sentTime = QDateTime::currentDateTime();
            event.setStartTime(sentTime);
            event.setEndTime(sentTime);

            event.setReportDelivery(true);

            DEBUG() << "Message recovered from delivery-echo" << event.toString();
        }
    }


    if (!messageFound) {
        qWarning() << "[DELIVERY] Failed to fetch event by message token/id";
        return result;
    }

    DEBUG() << "[DELIVERY] Event match: id:" << event.id()
             << "token:" << event.messageToken();

    // If variant is not valid, then we cant update status
    if (!status.isValid())
        return result;

    QDateTime deliveryTime;
    if (message.sent().isValid())
        deliveryTime = message.sent();
    else if (message.received().isValid())
        deliveryTime = message.received();
    else
        deliveryTime = QDateTime::currentDateTime();

    int deliveryStatus = status.value<int>();
    DEBUG() << "[DELIVERY] Message delivery status: " << deliveryStatus;

    switch (deliveryStatus) {
    case Tp::DeliveryStatusDelivered: {
        event.setStatus(CommHistory::Event::DeliveredStatus);
        event.setStartTime(deliveryTime);

        break;
    }
    case Tp::DeliveryStatusAccepted: {
        event.setStatus(CommHistory::Event::SentStatus);
        event.setStartTime(deliveryTime);
        break;
    }
    case Tp::DeliveryStatusTemporarilyFailed:
    case Tp::DeliveryStatusPermanentlyFailed: {
        // If sending fails, event is marked as temporarily failed
        // If delivery fails, event is marked as permanently failed
        event.setStartTime(deliveryTime);
        if (event.reportDelivery()
            && event.status() == CommHistory::Event::SentStatus) {
            event.setStatus(CommHistory::Event::PermanentlyFailedStatus);
        } else {
            event.setStatus(CommHistory::Event::TemporarilyFailedStatus);
        }
        handleMessageFailed(message, event);

        break;
    }
    case Tp::DeliveryStatusRead: {
        event.setStatus(CommHistory::Event::DeliveredStatus); // Message is read by recipient so it defenetelly delivered
        event.setStartTime(deliveryTime);
        event.setReadStatus(CommHistory::Event::ReadStatusRead);

        break;
    }
    case Tp::DeliveryStatusDeleted: {
        event.setStatus(CommHistory::Event::DeliveredStatus); // Message is read by recipient so it defenetelly delivered
        event.setStartTime(deliveryTime);
        event.setReadStatus(CommHistory::Event::ReadStatusDeleted);

        break;
    }
    default:
        // Unknown, cases are handled here.
        event.setStatus(CommHistory::Event::SendingStatus);

        break;
    }
    result = DeliveryHandlingResolved;

    return result;
}

bool TextChannelListener::areRemotePartiesOffline()
{
    bool offline = true;
    QHashIterator<QString,Presence> presenceIt(m_PresenceStatuses);
    while (presenceIt.hasNext()) {
        if (presenceIt.next().value().first != Tp::Presence::offline().status()) {
            offline = false;
            break;
        }
    }
    return offline;
}

void TextChannelListener::handleMessageFailed(const Tp::ReceivedMessage &message,
                                              const CommHistory::Event &event)
{
    DEBUG() << __PRETTY_FUNCTION__ << "message type:" << message.messageType();

    // if the received message is a delivery report
    if (message.messageType() == Tp::ChannelTextMessageTypeDeliveryReport) {

        // see if the message is actually an error message
        Tp::MessagePart part = message.header();
        int status = part.value(DELIVERY_STATUS).variant().toInt();
        QString messageToken = part.value(DELIVERY_TOKEN).variant().toString();
        QString dbusError = part.value(DELIVERY_DBUSERROR).variant().toString();
        QString errorMessage = part.value(DELIVERY_ERRORMESSAGE).variant().toString();

        DEBUG() << "status:"        << status
                 << "message token:" << messageToken
                 << "dbus error:"    << dbusError
                 << "error message:" << errorMessage;

        // dont show notes for perm. failed mms messages
        if(event.type() == CommHistory::Event::MMSEvent &&
           status == Tp::DeliveryStatusPermanentlyFailed) {
            return;
        }

        if (status == Tp::DeliveryStatusTemporarilyFailed ||
            status == Tp::DeliveryStatusPermanentlyFailed) {

            QString recipient;
            if (!event.contactName().isEmpty()) {
                // resolved name
                recipient = event.contactName();
            } else if (CommHistory::localUidComparesPhoneNumbers(event.localUid())) {
                // phone number
                ML10N::MLocale locale;
                recipient = locale.toLocalizedNumbers(event.recipients().value(0).remoteUid());
                recipient.insert(0, QChar(0x202A)); // left-to-right embedding
                recipient.append(QChar(0x202C)); // pop directional formatting
            } else {
                // IM address
                recipient = event.recipients().value(0).remoteUid();
            }

            // general sms error
            QString errorMsgToUser = txt_qtn_msg_error_sms_sending_failed(recipient);
            QString category = StrongErrorCategory;

            // check for specific error:
            // missing smsc
            if (dbusError == MODEM_ERROR_SMSC_ADDRESS_NOT_AVAILABLE) {

                errorMsgToUser = txt_qtn_msg_error_missing_smsc;
                category = ErrorCategory;
            }
            // fdn error
            else if (dbusError == MODEM_ERROR_DESTINATION_ADDRESS_FDN_RESTRICTED ||
                     dbusError == MODEM_ERROR_SMS_ADDRESS_FDN_RESTRICTED) {

                errorMsgToUser = txt_qtn_re_error_denied_phone_number(recipient);
                category = ErrorCategory;
            }
            // offline chatting
            else if (event.type() == CommHistory::Event::IMEvent
                     && areRemotePartiesOffline()) {

                errorMsgToUser = txt_qtn_msg_general_does_not_support_offline;
                category = ErrorCategory;
            }
            // IM message error
            else if (event.type() == CommHistory::Event::IMEvent) {
                errorMsgToUser = txt_qtn_msg_error_sending_failed(recipient);
                category = ErrorCategory;
            }

            DEBUG() << "error message shown to user:" << errorMsgToUser;
            showErrorNote(errorMsgToUser, category);
        }
    }
}

void TextChannelListener::showErrorNote(const QString &errorMsg, const QString &category)
{
    if (!errorMsg.isEmpty()) {
        Notification notification;
        notification.setAppName(txt_qtn_msg_errors_group);
        notification.setCategory(category);
        notification.setBody(errorMsg);
        notification.setPreviewBody(errorMsg);
        notification.publish();
    }
}

CommHistory::Event::EventType TextChannelListener::eventType() const
{
    CommHistory::Event::EventType type = CommHistory::Event::UnknownType;
    if (m_Account->protocolName() == PROTOCOL_TEL) {
        if (m_isClassZeroSMS) {
            type = CommHistory::Event::ClassZeroSMSEvent;
        } else {
            type = CommHistory::Event::SMSEvent;
        }
    } else {
        type = CommHistory::Event::IMEvent;
    }
    return type;
}

void TextChannelListener::fillEventFromMessage(const Tp::Message &message,
                                               CommHistory::Event &event)
{
    event.setType(eventType());

    // Check for possible sms-replace-number header in Tp::Message:
    QString replaceTypeString = replaceType(message.header());
    if (!replaceTypeString.isEmpty()) {
        QHash<QString, QString> replaceTypeHeader;
        replaceTypeHeader.insert(REPLACE_TYPE, replaceTypeString);
        event.setHeaders(replaceTypeHeader);
    }

    event.setLocalUid(m_Account->objectPath());
    event.setFreeText(message.text().trimmed());

    // do not set / create group id for class0 messages
    if (!m_isClassZeroSMS) {
        event.setGroupId(groupId());
    }
}

void TextChannelListener::handleReceivedMessage(const Tp::ReceivedMessage &message,
                                                CommHistory::Event &event)
{
    QString remoteId;
    QString messageText = message.text();

    uint handle = message.sender()->handle().first();

    bool fromSelf = false;
    if (m_Connection && m_Connection->isReady() && m_Connection->selfHandle() == handle) {
        fromSelf = true;
        remoteId = targetId();
    } else if (m_HandleOwnerNames.contains(handle))
        remoteId = m_HandleOwnerNames.value(handle);
    else if (message.sender())
        remoteId = message.sender()->id();
    else {
        qWarning() << "Message sender is unknown, use target id";
        remoteId = targetId();
    }

    DEBUG() << "Handling received message: " << remoteId << (fromSelf ? "<-" : "->")
             << m_Account->objectPath() << messageText;

    fillEventFromMessage(message, event);
    event.setRecipients(Recipient(m_Account->objectPath(), remoteId));

    if (fromSelf) {
        event.setDirection(CommHistory::Event::Outbound);
        event.setIsRead(true);
    } else {
        event.setDirection(CommHistory::Event::Inbound);
    }

    Tp::DeliveryStatus deliveryStatus = message.deliveryDetails().status();
    if (deliveryStatus == Tp::DeliveryStatusRead)
        event.setIsRead(true);

    QDateTime receivedTime;
    if (message.received().isValid())
        receivedTime = message.received();
    else
        receivedTime = QDateTime::currentDateTime();

    QDateTime sentTime;
    if (message.sent().isValid())
        sentTime = message.sent();
    else
        sentTime = receivedTime;

    event.setStartTime(sentTime);
    event.setEndTime(receivedTime);

    event.setMessageToken(message.messageToken());
    DEBUG() << "Message token is: " << message.messageToken();

    const Tp::MessagePart &header(message.header());
    const QString subscriberId(subscriberIdentity(header));
    if (!subscriberId.isEmpty())
        event.setSubscriberIdentity(subscriberId);
}

void TextChannelListener::slotMessageSent(const Tp::Message &message,
                                        Tp::MessageSendingFlags flags,
                                        const QString &messageToken)
{
    QString messageText = message.text();
    QString remoteUid = targetId();

    if (remoteUid.isEmpty())
        qCritical() << "Empty target id";

    int existingEventId = message.header().value("x-commhistory-event-id", QDBusVariant(-1)).variant().toInt();
    DEBUG() << "Handling sent message: " << m_Account->objectPath() << "->" << remoteUid << messageText;

    CommHistory::Event event;
    if (existingEventId >= 0 && getEventById(existingEventId, event) && event.isValid()) {
        DEBUG() << "Sent message has an existing event" << existingEventId;
    } else {
        fillEventFromMessage(message, event);
        event.setIsRead(true);
        event.setDirection(CommHistory::Event::Outbound);
        event.setRecipients(Recipient(m_Account->objectPath(), remoteUid));
        if (message.messageType() == Tp::ChannelTextMessageTypeAction)
            event.setIsAction(true);
    }

    QDateTime sentTime;
    if (message.sent().isValid()) {
        sentTime = message.sent();
    } else {
        sentTime = QDateTime::currentDateTime();
    }
    // TODO: think about start/endtime...
    event.setStartTime(sentTime);
    event.setEndTime(sentTime);

    event.setMessageToken(messageToken);
    DEBUG() << "Message token is: " << messageToken;

    const QVariantMap properties = m_Channel->immutableProperties();
    const QVariant &siProp = properties.value(SUBSCRIBER_ID_PROPERTY_NAME);
    if (siProp.isValid()) {
        event.setSubscriberIdentity(siProp.toString());
    }

    if (m_Account->protocolName() == PROTOCOL_TEL) {
        // Flag the message as Sending-in-progress
        event.setStatus(CommHistory::Event::SendingStatus);
    } else {
        // Flag the message as Sent, since we won't receive any further confirmation of delivery
        event.setStatus(CommHistory::Event::SentStatus);
    }

    // check if Event is sms & Report_Delivery flag set
    if (flags.testFlag(Tp::MessageSendingFlagReportDelivery)) {
        event.setReportDelivery(true);
    }

    saveMessage(event);

    if (event.type() == CommHistory::Event::IMEvent
        && areRemotePartiesOffline()
        && m_ShowOfflineChatError) {
        if (!m_IsGroupChat) {
            showErrorNote(txt_qtn_msg_general_supports_offline);
        } else {
            showErrorNote(txt_qtn_msg_all_participants_offline);
        }
        m_ShowOfflineChatError = false;
    }
}

void TextChannelListener::saveMessage(CommHistory::Event &event)
{
    DEBUG() << Q_FUNC_INFO << event.toString();

    if (event.id() >= 0) {
        if (!eventModel().modifyEvent(event)) {
            qWarning() << "failed to modify event";
            return;
        }
    } else {
        if (!eventModel().addEvent(event)) {
            qWarning() << "failed to add event";
            return;
        }
    }
}

void TextChannelListener::expungeMessage(const QString &token)
{
    if (checkStoredMessagesIf() && !token.isEmpty()) {
        if (m_expungeTokens.isEmpty()) {
            QTimer::singleShot(0, this, SLOT(slotExpungeMessages()));
        }
        m_expungeTokens.append(token);
    }
}

void TextChannelListener::updateGroupChatName(ChangedChannelProperty changedChannelProperty,
                                              bool suppressGroupChatEvents)
{
    DEBUG() << Q_FUNC_INFO;

    if (changedChannelProperty == ChannelName)
        m_GroupChatName = m_ChannelName;

    else if (changedChannelProperty == ChannelSubject)
        m_GroupChatName = m_ChannelSubject;

    DEBUG() << Q_FUNC_INFO << "New group chat name is" << m_GroupChatName;

    if (m_Group.isValid()) {

        DEBUG() << Q_FUNC_INFO << "Current group chat name for this group is" << m_Group.chatName();

        if (m_GroupChatName != m_Group.chatName()) {

            DEBUG() << Q_FUNC_INFO << "updating group chat name...";
            m_Group.setChatName(m_GroupChatName);

            if (m_GroupModel) {

                // Group is already in the database
                CommHistory::Group modGroup;
                modGroup.setId(m_Group.id());
                modGroup.setChatName(m_Group.chatName());
                if (!m_GroupModel->modifyGroup(modGroup))
                    qCritical() << "failed to modify group in database";

                if (suppressGroupChatEvents) {
                    DEBUG() << Q_FUNC_INFO << "NOT creating group chat event";
                    return;
                }

                QString remoteId;
                // Here we need to figure out who changed the topic
                if (m_HandleOwnerNames.contains(m_ChannelSubjectContactHandle))
                    remoteId = m_HandleOwnerNames.value(m_ChannelSubjectContactHandle);

                else
                    foreach (Tp::ContactPtr contact, m_Channel->groupContacts())
                        if (contact != m_Channel->groupSelfContact()) {

                            // TODO: can contact have more than one handle in this case???
                            QList<uint> handles = contact->handle().toList();
                            if (handles.first() == m_ChannelSubjectContactHandle)
                                remoteId = contact->alias();
                        }


                DEBUG() << Q_FUNC_INFO << "Chat room topic was changed by" << remoteId;

                // Create a temporary CommHistory::Event for showing chat room
                // topic change to the user in MUI conversation thread
                QString messageText;

                // if remoteId is empty, then YOU changed the topic
                if (remoteId.isEmpty()) {

                    // if new chat name is empty, then the topic was removed
                    if (m_GroupChatName.isEmpty())
                        messageText = txt_qtn_msg_group_chat_topic_cleared_user;

                    // otherwise a new topic was set
                    else
                        messageText = txt_qtn_msg_group_chat_topic_user_changed(m_GroupChatName);
                }
                // otherwise a remote party
                else {

                    if (m_GroupChatName.isEmpty())
                        messageText = txt_qtn_msg_group_chat_topic_cleared(remoteId);

                    else
                        messageText = txt_qtn_msg_group_chat_topic_changed(remoteId, m_GroupChatName);
                }

                sendGroupChatEvent(messageText);
            }
        }
    }
}

void TextChannelListener::sendGroupChatEvent(const QString &message)
{
    CommHistory::Event event;
    event.setType( CommHistory::Event::StatusMessageEvent );
    event.setDirection( CommHistory::Event::Inbound );
    event.setGroupId( m_Group.id() );
    event.setLocalUid( m_Account->objectPath() );
    event.setFreeText( message );
    event.setStartTime( QDateTime::currentDateTime() );
    event.setEndTime( QDateTime::currentDateTime() );
    event.setIsRead( true );

    if ( !eventModel().addEvent( event, true ) )
    {
        DEBUG() << "*** Adding group chat event message to data model has been failed.";
     }
}

void TextChannelListener::updateCurrentGroup(int start, int end, const QModelIndex &parent)
{
    DEBUG() << __PRETTY_FUNCTION__ << start << end;

    const Recipient recipient(m_Account->objectPath(), targetId());
    int fallbackRow = -1;
    int row = start;
    for ( ; row <= end; row++) {
        const QModelIndex &index = m_GroupModel->index(row, 0, parent);
        const CommHistory::Group &group = m_GroupModel->group(index);

        DEBUG() << Q_FUNC_INFO << "Inserted group's account: " << group.localUid();
        DEBUG() << Q_FUNC_INFO << "Inserted group's first target: " << group.recipients().value(0).remoteUid();

        if (group.isValid()) {
            const CommHistory::RecipientList &recipients = group.recipients();
            if (recipients.count() > 1) {
                DEBUG() << Q_FUNC_INFO << "has multiple recipients" << recipients.count();
                // This is a multi-member group; prefer to continue searching for an exact match
                if (fallbackRow == -1 && recipients.containsMatch(recipient)) {
                    DEBUG() << Q_FUNC_INFO << "set fallbackRow" << fallbackRow;
                    fallbackRow = row;
                }
            } else if (recipients.containsMatch(recipient)) {
                m_Group = group;
                DEBUG() << Q_FUNC_INFO << "found existing group:" << m_Group.id();
                break;
            }
        }
    }
    if (row == end) {
        if (fallbackRow != -1) {
            m_Group = m_GroupModel->group(m_GroupModel->index(fallbackRow, 0));
            DEBUG() << Q_FUNC_INFO << "found existing multi-member group:" << m_Group.id();
        } else {
            DEBUG() << Q_FUNC_INFO << "no existing group found for targetId:" << targetId();
        }
    }
}

void TextChannelListener::slotEventsCommitted(QList<CommHistory::Event> events, bool status)
{
    DEBUG() << Q_FUNC_INFO << status;

    bool removed = false;
    foreach (CommHistory::Event e, events) {
        if (m_EventTokens.contains(e.id())) {
            QString token = m_EventTokens.values(e.id()).last();
            if (status)
                expungeMessage(token);
            m_EventTokens.remove(e.id(), token);
        }
        if (m_commitingEvents.remove(e.messageToken()))
            removed = true;
    }

    if (!status) {
        qCritical() << "Failed to save message";
        // try to redeliver incoming messages
        if (!events.isEmpty()
            && events.first().direction() == CommHistory::Event::Inbound) {
            if (m_FailedSaveCount++ < MAX_SAVE_ATTEMPTS) {
                m_failedSaveEvents << events;
                QTimer::singleShot(m_FailedSaveCount*RESAVE_INTERVAL,
                                   this,
                                   SLOT(slotSaveFailedEvents()));
            } else {
               emit savingFailed(m_Connection);
           }
        }
    }

    // handle delivery reports pending for event commits
    if (removed)
        handleMessages();

    tryToClose();
}

void TextChannelListener::slotSaveFailedEvents()
{
    DEBUG() << Q_FUNC_INFO;
    if (eventModel().addEvents(m_failedSaveEvents)) {
        foreach (CommHistory::Event e, m_failedSaveEvents)
            m_EventTokens.insertMulti(e.id(), e.messageToken());
    }
    m_failedSaveEvents.clear();
}

void TextChannelListener::slotExpungeMessages()
{
    if (m_expungeTokens.isEmpty()
        || (m_Connection && !m_Connection->isReady()))
        return;

    CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface* storedMessages =
            m_Connection->interface<CommHistoryTp::Client::ConnectionInterfaceStoredMessagesInterface>();

    if (storedMessages) {
        DEBUG() << Q_FUNC_INFO << m_expungeTokens;
        storedMessages->ExpungeMessages(m_expungeTokens);
        m_expungeTokens.clear();
    } else {
        qCritical() << Q_FUNC_INFO << "No stored messages interface present";
    }

    tryToClose();
}

void TextChannelListener::slotPresenceChanged(const Tp::Presence &presence)
{
    DEBUG() << Q_FUNC_INFO;

    Tp::Contact *contact = qobject_cast<Tp::Contact *>(sender());
    if (!contact) {
        qWarning() << Q_FUNC_INFO << ": invalid contact";
        return;
    }

    Presence oldPresenceValues = m_PresenceStatuses.value(contact->id());
    m_PresenceStatuses.insert(contact->id(),Presence(presence.status(), presence.statusMessage()));

    /* If offline chat error has already been shown and presence status changed
       and we have at least one participant online after this status change
       then offline chat error is allowed to be shown next time: */
    if (!m_ShowOfflineChatError && (oldPresenceValues.first != presence.status()) && !areRemotePartiesOffline())
        m_ShowOfflineChatError = true;

    // Status message did not change, do not send status message event.
    if (oldPresenceValues.second == presence.statusMessage())
        return;

    QString remoteId;
    uint handle = contact->handle().first();
    if (m_HandleOwnerNames.contains(handle))
        remoteId = m_HandleOwnerNames.value(handle);
    else
        remoteId = contact->id();

    QString newStatusMessage = m_PresenceStatuses.value(contact->id()).second;

    if (!newStatusMessage.isEmpty() && groupId() != -1) {
        DEBUG() << Q_FUNC_INFO << "Preparing status message event.";
        CommHistory::Event event;
        event.setType(CommHistory::Event::StatusMessageEvent);
        event.setDirection(CommHistory::Event::Inbound);
        event.setRecipients(Recipient(m_Account->objectPath(), remoteId));
        event.setGroupId(groupId());
        event.setLocalUid(m_Account->objectPath());
        event.setFreeText(newStatusMessage);
        event.setStartTime(QDateTime::currentDateTime());
        event.setEndTime(QDateTime::currentDateTime());
        event.setIsRead(true);

        if (!eventModel().addEvent(event, true)) {

            DEBUG() << "*** Adding status message to data model has been failed.";
        }
    }
}

void TextChannelListener::channelReady()
{
    DEBUG() << Q_FUNC_INFO;

    if (m_Channel && m_Connection) {
        if (m_Channel->targetHandleType() == Tp::HandleTypeRoom) {
            DEBUG() << Q_FUNC_INFO << "group chat: HandleTypeRoom";
            m_IsGroupChat = true;
            m_GroupHandleType = Tp::HandleTypeRoom;
        } else if (m_Channel->targetHandleType() == Tp::HandleTypeNone
                   && m_Channel->interfaces().contains(
                       TP_QT_IFACE_CHANNEL_INTERFACE_GROUP)) {
            m_IsGroupChat = true;
            m_GroupHandleType = Tp::HandleTypeNone;
            m_PersistentId = m_Channel->immutableProperties().value(
                TELEPATHY_CHANNEL_INTERFACE_PERSISTENT_ID).toString();
            DEBUG() << Q_FUNC_INFO << "group chat: HandleTypeNone, PersistentId ="
                     << m_PersistentId;

            if (m_PersistentId.isEmpty()) {
                qCritical() << Q_FUNC_INFO << "No persistent id for Tp::HandleTypeNone groupchat";
                return;
            }
        }

        if (m_Channel->interfaces().contains(TP_QT_IFACE_CHANNEL_INTERFACE_GROUP))
            connect(m_Channel.data(),
                    SIGNAL(groupMembersChanged(const Tp::Contacts &,
                                               const Tp::Contacts &,
                                               const Tp::Contacts &,
                                               const Tp::Contacts &,
                                               const Tp::Channel::GroupMemberChangeDetails &)),
                    this,
                    SLOT(slotGroupMembersChanged(const Tp::Contacts &,
                                                 const Tp::Contacts &,
                                                 const Tp::Contacts &,
                                                 const Tp::Contacts &,
                                                 const Tp::Channel::GroupMemberChangeDetails &)));

        // request contact for target id to track presence
        Tp::ContactManagerPtr contactManager = m_Connection->contactManager();
        if (contactManager->supportedFeatures().contains(Tp::Contact::FeatureSimplePresence)) {
            Tp::UIntList handles;
            handles << m_Channel->targetHandle();

            Tp::Features features;
            features << Tp::Contact::FeatureSimplePresence;
            features << Tp::Contact::FeatureAlias;

            Tp::PendingContacts* pendingContacts = contactManager->contactsForHandles(handles,
                                                                                      features);
            connect(pendingContacts,
                    SIGNAL(finished(Tp::PendingOperation*)),
                    this,
                    SLOT(slotContactsReady(Tp::PendingOperation*)));

            if (m_IsGroupChat) {
                QList<Tp::ContactPtr> contactList;

                foreach (Tp::ContactPtr contact, m_Channel->groupContacts()) {
                    if (contact != m_Channel->groupSelfContact())
                        contactList << contact;
                }

                if (!contactList.isEmpty()) {
                    Tp::PendingContacts *pc = contactManager->upgradeContacts(contactList,
                                                                              features);
                    connect(pc, SIGNAL(finished(Tp::PendingOperation *)),
                            this, SLOT(slotContactsReady(Tp::PendingOperation *)));
                }

                if (m_Channel->groupAreHandleOwnersAvailable()) {
                    Tp::UIntList handleOwnerList;
                    foreach (Tp::ContactPtr contact, contactList) {
                        uint ownerHandle = m_Channel->groupHandleOwners().
                            value(contact->handle().first());
                        if (ownerHandle)
                            handleOwnerList << ownerHandle;
                    }

                    if (!handleOwnerList.isEmpty()) {
                        Tp::PendingContacts *owners =
                            contactManager->contactsForHandles(handleOwnerList);
                        connect(owners, SIGNAL(finished(Tp::PendingOperation *)),
                                this, SLOT(slotHandleOwnersFetched(Tp::PendingOperation *)));
                    }
                }

                connect(m_Channel.data(),
                        SIGNAL(groupHandleOwnersChanged(const Tp::HandleOwnerMap &,
                                                        const Tp::UIntList &,
                                                        const Tp::UIntList &)),
                        this,
                        SLOT(slotHandleOwnersChanged(const Tp::HandleOwnerMap &,
                                                     const Tp::UIntList &,
                                                     const Tp::UIntList &)));
            }
        }
        checkStoredMessagesIf();
        connect(&eventModel(), SIGNAL(eventsCommitted(QList<CommHistory::Event>,bool)),
                SLOT(slotEventsCommitted(QList<CommHistory::Event>,bool)),
                (Qt::ConnectionType) (Qt::UniqueConnection | Qt::QueuedConnection));
        // call this last as it may start handle pending messages
        requestConversationId();
    } else {
        qCritical() << Q_FUNC_INFO << "Invalid channel: " << m_Channel.data()
                    << " or connection " << m_Connection.data();
    }
}

void TextChannelListener::slotContactsReady(Tp::PendingOperation* operation)
{
    DEBUG() << Q_FUNC_INFO << channel();

    if (operation && operation->isError()) {
        qWarning() << "No presence contacts" << operation->errorMessage();
        return;
    }

    Tp::PendingContacts* pendingContacts = static_cast<Tp::PendingContacts*>(operation);
    if (pendingContacts) {
        QList<Tp::ContactPtr> contacts(pendingContacts->contacts());

        for ( int i = 0; i < contacts.count(); i++ )
        {
            // initialise presence values:
            if(!contacts.value(i).isNull()) {
                m_PresenceStatuses.insert(contacts.value(i)->id(),Presence(contacts.value(i)->presence().status(),
                                          contacts.value(i)->presence().statusMessage()));
            }

            connect(contacts.value(i).data(),
                    SIGNAL(presenceChanged(const Tp::Presence &)),
                    SLOT(slotPresenceChanged(const Tp::Presence &)));
        }
    } else {
        qCritical() << Q_FUNC_INFO << "Invalid TpPendingOperation when requesting target contact";
    }
}

void TextChannelListener::slotPropertiesChanged(const Tp::PropertyValueList &props, bool listProps)
{
    DEBUG() << Q_FUNC_INFO << listProps;
    ChangedChannelProperty changedProperty = None;

    foreach (Tp::PropertyValue value, props) {
        if (m_Properties.contains(CHANNEL_PROPERTY_NAME)) {
            if (value.identifier == m_Properties.value(CHANNEL_PROPERTY_NAME).propertyID) {
                m_ChannelName = value.value.variant().toString();
                changedProperty = ChannelName;
                DEBUG() << Q_FUNC_INFO << "name changed:" << m_ChannelName;
            }
        }
        if (m_Properties.contains(CHANNEL_PROPERTY_SUBJECT)) {
            if (value.identifier == m_Properties.value(CHANNEL_PROPERTY_SUBJECT).propertyID) {
                m_ChannelSubject = value.value.variant().toString();
                changedProperty = ChannelSubject;
                DEBUG() << Q_FUNC_INFO << "subject changed:" << m_ChannelSubject;
            }
        }
        if (m_Properties.contains(CHANNEL_PROPERTY_SUBJECT_CONTACT)) {
            if (value.identifier == m_Properties.value(CHANNEL_PROPERTY_SUBJECT_CONTACT).propertyID) {
                m_ChannelSubjectContactHandle = value.value.variant().toUInt();
                DEBUG() << Q_FUNC_INFO << "handle of the contact who changed the subject:" << m_ChannelSubjectContactHandle;
            }
        }
    }

    if (changedProperty != None)
        updateGroupChatName(changedProperty, listProps);
}

void TextChannelListener::slotGroupMembersChanged(
        const Tp::Contacts &groupMembersAdded,
        const Tp::Contacts &groupLocalPendingMembersAdded,
        const Tp::Contacts &groupRemotePendingMembersAdded,
        const Tp::Contacts &groupMembersRemoved,
        const Tp::Channel::GroupMemberChangeDetails &details)
{
    DEBUG() << Q_FUNC_INFO;

    Q_UNUSED(groupLocalPendingMembersAdded);
    Q_UNUSED(groupRemotePendingMembersAdded);

    if (!groupMembersRemoved.isEmpty()) {

        foreach (Tp::ContactPtr contact, groupMembersRemoved) {

            // if there is valid originatior for the change
            // but the originator is not the same as the person removed
            // and the reason is kick/ban
            if (/*details.isValid() &&*/
                !details.actor().isNull() &&
                details.actor() != contact &&
                (details.reason() == Tp::ChannelGroupChangeReasonKicked ||
                 details.reason() == Tp::ChannelGroupChangeReasonBanned)) {

                if (contact == m_Channel->groupSelfContact()) {

                    DEBUG() << "YOU've been banned/kicked by" << details.actor()->alias();
                    sendGroupChatEvent(txt_qtn_msg_group_chat_you_removed(details.actor()->alias()));
                }
                else {

                    DEBUG() << contact->alias() << "has been banned/kicked by" << details.actor()->alias();
                    sendGroupChatEvent(txt_qtn_msg_group_chat_person_removed(details.actor()->alias(), contact->alias()));
                    m_PresenceStatuses.remove(contact->id());
                }
            }

            // otherwise fall back to normal _leave_
            else {

                DEBUG() << contact->alias() << "has left the channel";
                sendGroupChatEvent(txt_qtn_msg_group_chat_remote_left(contact->alias()));
                m_PresenceStatuses.remove(contact->id());
            }
        }
    }

    if (!groupMembersAdded.isEmpty()) {

        Tp::Features features;
        features << Tp::Contact::FeatureSimplePresence;
        features << Tp::Contact::FeatureAlias;

        Tp::PendingContacts *pc = m_Channel->connection()->contactManager()->upgradeContacts(
            QList<Tp::ContactPtr>::fromSet(groupMembersAdded), features);

        connect(pc,
                SIGNAL(finished(Tp::PendingOperation *)),
                this,
                SLOT(slotContactsReady(Tp::PendingOperation *)));

        connect(pc,
                SIGNAL(finished(Tp::PendingOperation *)),
                this,
                SLOT(slotJoinedGroupChat(Tp::PendingOperation *)));
    }
}

void TextChannelListener::slotJoinedGroupChat(Tp::PendingOperation *operation)
{
    DEBUG() << Q_FUNC_INFO << channel();

    if (operation && operation->isError()) {
        qWarning() << "No contacts" << operation->errorMessage();
        return;
    }

    Tp::PendingContacts* pendingContacts = static_cast<Tp::PendingContacts *>(operation);
    if (pendingContacts) {
        QList<Tp::ContactPtr> contacts(pendingContacts->contacts());
        for (int i = 0; i < contacts.count(); i++) {
            // Ignore self contact. In that case "You have joined..." message
            // should be shown instead (by messaging-ui)
            if (contacts.value(i) != m_Channel->groupSelfContact()) {
                DEBUG() << contacts.value(i)->alias() << "joined";
                sendGroupChatEvent(txt_qtn_msg_group_chat_remote_joined(contacts.value(i)->alias()));
            }
        }
    }
}

void TextChannelListener::slotHandleOwnersFetched(Tp::PendingOperation *op)
{
    Tp::PendingContacts *pc = qobject_cast<Tp::PendingContacts *>(op);

    if (pc && !pc->isError()) {
        foreach (Tp::ContactPtr handleOwner, pc->contacts()) {
            QMapIterator<uint, uint> i(m_Channel->groupHandleOwners());
            while (i.hasNext()) {
                i.next();
                if (i.value() == handleOwner->handle().first()) {
                    m_HandleOwnerNames.insert(i.key(), handleOwner->id());
                    DEBUG() << Q_FUNC_INFO << "added handle owner:"
                             << i.key() << handleOwner->id();
                    break;
                }
            }
        }
    }
}

void TextChannelListener::slotHandleOwnersChanged(const Tp::HandleOwnerMap &handleOwnerMap,
                                                  const Tp::UIntList &added,
                                                  const Tp::UIntList &removed)
{
    if (!added.isEmpty()) {
        Tp::UIntList addedOwners;
        foreach (uint handle, added)
            addedOwners << handleOwnerMap.value(handle);
        Tp::PendingContacts *owners =
            m_Channel->connection()->contactManager()->contactsForHandles(addedOwners);
        connect(owners, SIGNAL(finished(Tp::PendingOperation *)),
                this, SLOT(slotHandleOwnersFetched(Tp::PendingOperation *)));
    }

    foreach (uint handle, removed)
        m_HandleOwnerNames.remove(handle);
}

bool TextChannelListener::hasPendingOperations() const
{
    return !(m_expungeTokens.isEmpty()
             && m_EventTokens.isEmpty()
             && m_pendingGroups.isEmpty()
             && m_failedSaveEvents.isEmpty()
             && m_replaceMessages.isEmpty());
}

void TextChannelListener::tryToClose()
{
    if (m_channelClosed && !hasPendingOperations()) {
        closed();
    }
}

void TextChannelListener::closed()
{
    m_pendingMessageIds.remove(m_Channel->objectPath());
    ChannelListener::closed();
}

void TextChannelListener::finishedWithError(const QString& errorName,
                                            const QString& errorMessage)
{
    DEBUG() << Q_FUNC_INFO;
    if (!m_InvocationContext.isNull())
        m_InvocationContext->setFinishedWithError(errorName, errorMessage);

    m_channelClosed = true;

    tryToClose();
}

CommHistory::Group TextChannelListener::getGroupById(int groupId) const
{
    // Check most common case first
    if (m_Group.isValid() && m_Group.id() == groupId)
        return m_Group;

    if (!m_GroupModel || !m_GroupModel->isReady()) {
        qWarning() << Q_FUNC_INFO << "Can't read group model";
        return CommHistory::Group();
    }

    for (int row = 0; row < m_GroupModel->rowCount(); row++) {
        QModelIndex index = m_GroupModel->index(row, 0);
        CommHistory::Group group = m_GroupModel->group(index);
        if (group.isValid() && group.id() == groupId)
            return group;
    }

    qWarning() << Q_FUNC_INFO << "Didn't find matching group";
    return CommHistory::Group();
}

bool TextChannelListener::pendingCommit(const QString &messageToken)
{
    foreach (QString token, m_commitingEvents) {
        if (token == messageToken)
            return true;
    }

    return false;
}
