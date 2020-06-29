/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2013-2016 Jolla Ltd.
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

// Qt includes
#include <QCoreApplication>
#include <QDBusReply>
#include <QDir>

// CommHistory includes
#include <CommHistory/commonutils.h>
#include <CommHistory/GroupModel>
#include <CommHistory/Group>

// Telepathy includes
#include <TelepathyQt/Constants>

// NGF-Qt includes
#include <NgfClient>

// nemo notifications
#include <notification.h>

// mce
#include <mce/dbus-names.h>

// Our includes
#include "qofonomanager.h"
#include "notificationmanager.h"
#include "locstrings.h"
#include "constants.h"
#include "debug.h"

using namespace RTComLogger;
using namespace CommHistory;

NotificationManager* NotificationManager::m_pInstance = 0;

static const QString NgfdEventSms("sms");
static const QString NgfdEventChat("chat");

// constructor
//
NotificationManager::NotificationManager(QObject* parent)
        : QObject(parent)
        , m_Initialised(false)
        , m_contactResolver(0)
        , m_GroupModel(0)
        , m_ngfClient(0)
        , m_ngfEvent(0)
{
}

NotificationManager::~NotificationManager()
{
    qDeleteAll(interfaces.values());
    qDeleteAll(m_Groups);
    qDeleteAll(m_unresolvedNotifications);
}

void NotificationManager::addModem(QString path)
{
    DEBUG() << "NotificationManager::addModem" << path;
    QOfonoMessageWaiting *mw = new QOfonoMessageWaiting(this);
    interfaces.insert(path, mw);

    mw->setModemPath(path);

    connect(mw, SIGNAL(voicemailWaitingChanged(bool)), SLOT(slotVoicemailWaitingChanged()));
    connect(mw, SIGNAL(voicemailMessageCountChanged(int)), SLOT(slotVoicemailWaitingChanged()));
    connect(mw, SIGNAL(validChanged(bool)), this, SLOT(slotValidChanged(bool)));

    if (mw->isValid()) {
        DEBUG() << "NotificationManager::addModem, mwi interface already valid";
        slotVoicemailWaitingChanged();
    }
}

void NotificationManager::init()
{
    if (m_Initialised) {
        return;
    }

    m_contactResolver = new ContactResolver(this);
    connect(m_contactResolver, SIGNAL(finished()),
            SLOT(slotContactResolveFinished()));

    m_contactListener = ContactListener::instance();
    connect(m_contactListener.data(), SIGNAL(contactChanged(RecipientList)),
            SLOT(slotContactChanged(RecipientList)));
    connect(m_contactListener.data(), SIGNAL(contactInfoChanged(RecipientList)),
            SLOT(slotContactInfoChanged(RecipientList)));

    m_ngfClient = new Ngf::Client(this);
    connect(m_ngfClient, SIGNAL(eventFailed(quint32)), SLOT(slotNgfEventFinished(quint32)));
    connect(m_ngfClient, SIGNAL(eventCompleted(quint32)), SLOT(slotNgfEventFinished(quint32)));

    ofonoManager = QOfonoManager::instance();
    QOfonoManager* ofono = ofonoManager.data();
    connect(ofono, SIGNAL(modemsChanged(QStringList)), this, SLOT(slotModemsChanged(QStringList)));
    connect(ofono, SIGNAL(modemAdded(QString)), this, SLOT(slotModemAdded(QString)));
    connect(ofono, SIGNAL(modemRemoved(QString)), this, SLOT(slotModemRemoved(QString)));
    QStringList modems = ofono->modems();
    DEBUG() << "Created modem manager";
    foreach (QString path, modems) {
        addModem(path);
    }

    // Loads old state
    syncNotifications();

    CommHistoryService *service = CommHistoryService::instance();
    connect(service, SIGNAL(inboxObservedChanged(bool,QString)), SLOT(slotInboxObservedChanged()));
    connect(service, SIGNAL(callHistoryObservedChanged(bool)), SLOT(slotCallHistoryObservedChanged(bool)));
    connect(service, SIGNAL(observedConversationsChanged(QList<CommHistoryService::Conversation>)),
                     SLOT(slotObservedConversationsChanged(QList<CommHistoryService::Conversation>)));

    groupModel();

    m_Initialised = true;
}

void NotificationManager::syncNotifications()
{
    QList<PersonalNotification*> pnList;
    QMap<int,int> typeCounts;
    QList<QObject*> notifications = Notification::notifications();

    foreach (QObject *o, notifications) {
        Notification *n = static_cast<Notification*>(o);

        if (n->hintValue("x-commhistoryd-data").isNull()) {
            // This was a group notification, which will be recreated if required
            n->close();
            delete n;
        } else {
            PersonalNotification *pn = new PersonalNotification(this);
            if (!pn->restore(n)) {
                delete pn;
                n->close();
                delete n;
                continue;
            }

            typeCounts[pn->eventType()]++;
            pnList.append(pn);
        }
    }

    foreach (PersonalNotification *pn, pnList)
        resolveNotification(pn);
}

NotificationManager* NotificationManager::instance()
{
    if (!m_pInstance) {
        m_pInstance = new NotificationManager(QCoreApplication::instance());
        m_pInstance->init();
    }

    return m_pInstance;
}

bool NotificationManager::updateEditedEvent(const CommHistory::Event& event, const QString &text)
{
    if (event.messageToken().isEmpty())
        return false;

    foreach (PersonalNotification *notification, m_unresolvedNotifications) {
        if (notification->eventToken() == event.messageToken()) {
            notification->setNotificationText(text);
            return true;
        }
    }

    EventGroupProperties groupProperties(eventGroup(PersonalNotification::collection(event.type()), event.recipients().value(0)));
    NotificationGroup *eventGroup = m_Groups.value(groupProperties);
    if (!eventGroup)
        return false;

    foreach (PersonalNotification *pn, eventGroup->notifications()) {
        if (pn->eventToken() == event.messageToken()) {
            pn->setNotificationText(text);
            return true;
        }
    }

    return false;
}

void NotificationManager::showNotification(const CommHistory::Event& event,
                                           const QString& channelTargetId,
                                           CommHistory::Group::ChatType chatType,
                                           const QString &details)
{
    DEBUG() << Q_FUNC_INFO << event.id() << channelTargetId << chatType;

    if (event.type() == CommHistory::Event::SMSEvent
        || event.type() == CommHistory::Event::MMSEvent
        || event.type() == CommHistory::Event::IMEvent)
    {
        bool inboxObserved = CommHistoryService::instance()->inboxObserved();
        if (inboxObserved || isCurrentlyObservedByUI(event, channelTargetId, chatType)) {
            if (!m_ngfClient->isConnected())
                m_ngfClient->connect();

            if (!m_ngfEvent) {
                QMap<QString, QVariant> properties;
                properties.insert("play.mode", "foreground");
                const QString *ngfEvent;
                if (event.type() == CommHistory::Event::SMSEvent || event.type() == CommHistory::Event::MMSEvent) {
                    ngfEvent = &NgfdEventSms;
                } else {
                    ngfEvent = &NgfdEventChat;
                }
                DEBUG() << Q_FUNC_INFO << "play ngf event: " << ngfEvent;
                m_ngfEvent = m_ngfClient->play(*ngfEvent, properties);
            }

            return;
        }
    }

    // try to update notifications for existing event
    QString text(notificationText(event, details));
    if (event.isValid() && updateEditedEvent(event, text))
        return;

    // Get MUC topic from group
    QString chatName;
    if (m_GroupModel && (chatType == CommHistory::Group::ChatTypeUnnamed ||
        chatType == CommHistory::Group::ChatTypeRoom)) {
        for (int i = 0; i < m_GroupModel->rowCount(); i++) {
            QModelIndex row = m_GroupModel->index(i, 0);
            CommHistory::Group group = m_GroupModel->group(row);
            if (group.isValid() && group.id() == event.groupId()) {
                chatName = group.chatName();
                if (chatName.isEmpty())
                    chatName = txt_qtn_msg_group_chat;
                DEBUG() << Q_FUNC_INFO << "Using chatName:" << chatName;
                break;
            }
        }
    }

    PersonalNotification *notification = new PersonalNotification(event.recipients().value(0).remoteUid(),
            event.localUid(), event.type(), channelTargetId, chatType);
    notification->setNotificationText(text);
    notification->setSmsReplaceNumber(event.headers().value(REPLACE_TYPE));

    if (!chatName.isEmpty())
        notification->setChatName(chatName);

    notification->setEventToken(event.messageToken());

    resolveNotification(notification);
}

void NotificationManager::resolveNotification(PersonalNotification *pn)
{
    if (pn->remoteUid() == QLatin1String("<hidden>") ||
        !pn->chatName().isEmpty() ||
        pn->recipient().isContactResolved()) {
        // Add notification immediately
        addNotification(pn);
    } else {
        DEBUG() << Q_FUNC_INFO << "Trying to resolve contact for" << pn->account() << pn->remoteUid();
        m_unresolvedNotifications.append(pn);
        m_contactResolver->add(pn->recipient());
    }
}

void NotificationManager::playClass0SMSAlert()
{
    if (!m_ngfClient->isConnected())
        m_ngfClient->connect();

    m_ngfEvent = m_ngfClient->play(QLatin1Literal("sms"));

    // ask mce to undim the screen
    QString mceMethod = QString::fromLatin1(MCE_DISPLAY_ON_REQ);
    QDBusMessage msg = QDBusMessage::createMethodCall(MCE_SERVICE, MCE_REQUEST_PATH, MCE_REQUEST_IF, mceMethod);
    QDBusConnection::systemBus().call(msg, QDBus::NoBlock);
}

void NotificationManager::requestClass0Notification(const CommHistory::Event &event)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String("org.nemomobile.ClassZeroSmsNotification"),
                                                      QLatin1String("/org/nemomobile/ClassZeroSmsNotification"),
                                                      QLatin1String("org.nemomobile.ClassZeroSmsNotification"),
                                                      QLatin1String("showNotification"));
    QList<QVariant> arguments;
    arguments << event.freeText();
    msg.setArguments(arguments);
    if (!QDBusConnection::sessionBus().callWithCallback(msg, this, 0, SLOT(slotClassZeroError(QDBusError)))) {
        qWarning() << "Unable to create class 0 SMS notification request";
    }
}

bool NotificationManager::isCurrentlyObservedByUI(const CommHistory::Event& event,
                                                  const QString &channelTargetId,
                                                  CommHistory::Group::ChatType chatType)
{
    // Return false if it's not message event (IM or SMS/MMS)
    CommHistory::Event::EventType eventType = event.type();
    if (eventType != CommHistory::Event::IMEvent
        && eventType != CommHistory::Event::SMSEvent
        && eventType != CommHistory::Event::MMSEvent)
    {
        return false;
    }

    QString remoteMatch;
    if (chatType == CommHistory::Group::ChatTypeP2P)
        remoteMatch = event.recipients().value(0).remoteUid();
    else
        remoteMatch = channelTargetId;

    const Recipient messageRecipient(event.localUid(), remoteMatch);

    foreach (const CommHistoryService::Conversation &conversation, CommHistoryService::instance()->observedConversations()) {
        if (conversation.first.matches(messageRecipient) && conversation.second == chatType)
            return true;
    }

    return false;
}

void NotificationManager::removeNotifications(const QString &accountPath, const QList<int> &removeTypes)
{
    DEBUG() << Q_FUNC_INFO << "Removing notifications of account " << accountPath;

    QSet<NotificationGroup> updatedGroups;

    // remove matched notifications and update group
    foreach (NotificationGroup *group, m_Groups) {
        if (group->localUid() != accountPath) {
            continue;
        }

        foreach (PersonalNotification *notification, group->notifications()) {
            if (removeTypes.isEmpty() || removeTypes.contains(notification->eventType())) {
                DEBUG() << Q_FUNC_INFO << "Removing notification: accountPath: " << notification->account() << " remoteUid: " << notification->remoteUid();
                group->removeNotification(notification);
            }
        }
    }

    for (QList<PersonalNotification*>::iterator it = m_unresolvedNotifications.begin();
            it != m_unresolvedNotifications.end(); ) {
        if ((*it)->account() == accountPath) {
            delete *it;
            it = m_unresolvedNotifications.erase(it);
        } else
            it++;
    }
}

void NotificationManager::removeConversationNotifications(const CommHistory::Recipient &recipient,
                                                          CommHistory::Group::ChatType chatType)
{
    QHash<EventGroupProperties, NotificationGroup *>::const_iterator it = m_Groups.constBegin(), end = m_Groups.constEnd();
    for ( ; it != end; ++it) {
        NotificationGroup *group(it.value());

        foreach (PersonalNotification *notification, group->notifications()) {
            if (notification->collection() != PersonalNotification::Messaging ||
                notification->chatType() != chatType)
                continue;

            // For p-to-p chat we use remote uid for comparison and for MUC we use target (channel) id:
            bool match(false);
            if (chatType == CommHistory::Group::ChatTypeP2P)
                match = recipient.matches(notification->recipient());
            else
                match = recipient.matches(Recipient(notification->account(), notification->targetId()));

            if (match)
                group->removeNotification(notification);
        }
    }
}

void NotificationManager::slotObservedConversationsChanged(const QList<CommHistoryService::Conversation> &conversations)
{
    foreach (const CommHistoryService::Conversation &conversation, conversations) {
        removeConversationNotifications(conversation.first, static_cast<CommHistory::Group::ChatType>(conversation.second));
    }
}

void NotificationManager::slotInboxObservedChanged()
{
    DEBUG() << Q_FUNC_INFO;

    // Cannot be passed as a parameter, because this slot is also used for m_notificationTimer
    bool observed = CommHistoryService::instance()->inboxObserved();
    if (observed) {
        QList<int> removeTypes;
        removeTypes << CommHistory::Event::IMEvent << CommHistory::Event::SMSEvent << CommHistory::Event::MMSEvent << VOICEMAIL_SMS_EVENT_TYPE;

        if (!isFilteredInbox()) {
            // remove sms, mms and im notifications
            removeNotificationTypes(removeTypes);
        } else {
            // Filtering is in use, remove only notifications of that account whose threads are visible in inbox:
            QString filteredAccountPath = filteredInboxAccountPath();
            DEBUG() << Q_FUNC_INFO << "Removing only notifications belonging to account " << filteredAccountPath;
            if (!filteredAccountPath.isEmpty())
                removeNotifications(filteredAccountPath, removeTypes);
        }
    }
}

void NotificationManager::slotCallHistoryObservedChanged(bool observed)
{
    if (observed) {
        removeNotificationTypes(QList<int>() << CommHistory::Event::CallEvent);
    }
}

bool NotificationManager::isFilteredInbox()
{
    return !CommHistoryService::instance()->inboxFilterAccount().isEmpty();
}

QString NotificationManager::filteredInboxAccountPath()
{
    return CommHistoryService::instance()->inboxFilterAccount();
}

void NotificationManager::removeNotificationTypes(const QList<int> &types)
{
    DEBUG() << Q_FUNC_INFO << types;

    foreach (NotificationGroup *group, m_Groups) {
        foreach (PersonalNotification *notification, group->notifications()) {
            if (types.contains(notification->eventType())) {
                group->removeNotification(notification);
            }
        }
    }
}

void NotificationManager::addNotification(PersonalNotification *notification)
{
    EventGroupProperties groupProperties(eventGroup(notification->collection(), CommHistory::Recipient(notification->account(), notification->remoteUid())));
    NotificationGroup *group = m_Groups.value(groupProperties);
    if (!group) {
        group = new NotificationGroup(groupProperties.collection, notification->account(), notification->remoteUid(), this);
        m_Groups.insert(groupProperties, group);
    }

    group->addNotification(notification);
}

int NotificationManager::pendingEventCount()
{
    return m_unresolvedNotifications.size();
}

QString NotificationManager::notificationText(const CommHistory::Event& event, const QString &details)
{
    QString text;
    switch(event.type())
    {
        case CommHistory::Event::IMEvent:
        case CommHistory::Event::SMSEvent:
        {
            text = event.fromVCardLabel().isEmpty()
                   ? event.freeText()
                   : txt_qtn_msg_notification_new_vcard(event.fromVCardLabel());
            break;
        }
        case CommHistory::Event::MMSEvent:
        {
            if (event.status() == Event::ManualNotificationStatus) {
                text = txt_qtn_mms_notification_manual_download;
            } else if (event.status() >= Event::TemporarilyFailedStatus) {
                QString trimmedDetails(details.trimmed());
                if (trimmedDetails.isEmpty()) {
                    if (event.direction() == Event::Inbound)
                        text = txt_qtn_mms_notification_download_failed;
                    else
                        text = txt_qtn_mms_notification_send_failed;
                } else {
                    text = trimmedDetails;
                }
            } else {
                if (!event.subject().isEmpty())
                    text = event.subject();
                else
                    text = event.freeText();

                int attachmentCount = 0;
                foreach (const MessagePart &part, event.messageParts()) {
                    if (!part.contentType().startsWith("text/plain") &&
                        !part.contentType().startsWith("application/smil"))
                    {
                        attachmentCount++;
                    }
                }

                if (attachmentCount > 0) {
                    if (!text.isEmpty())
                        text = txt_qtn_mms_notification_with_text(attachmentCount, text);
                    else
                        text = txt_qtn_mms_notification_attachment(attachmentCount);
                }
            }
            break;
        }

        case CommHistory::Event::CallEvent:
        {
            text = txt_qtn_call_missed(1);
            break;
        }
        case CommHistory::Event::VoicemailEvent:
        {
            // freeText() returns the amount of new / not listened voicemails
            // e.g. 3 Voicemails
            text = event.freeText();
            break;
        }
        default:
            break;
    }

    return text;
}

static QVariant dbusAction(const QString &name, const QString &displayName, const QString &service, const QString &path, const QString &iface,
                           const QString &method, const QVariantList &arguments = QVariantList())
{
    return Notification::remoteAction(name, displayName, service, path, iface, method, arguments);
}

void NotificationManager::setNotificationProperties(Notification *notification, PersonalNotification *pn, bool grouped)
{
    QVariantList remoteActions;

    switch (pn->collection()) {
        case PersonalNotification::Messaging:

            if (pn->eventType() != VOICEMAIL_SMS_EVENT_TYPE && grouped) {
                remoteActions.append(dbusAction("default",
                                                txt_qtn_msg_notification_show_messages,
                                                MESSAGING_SERVICE_NAME,
                                                OBJECT_PATH,
                                                MESSAGING_INTERFACE,
                                                SHOW_INBOX_METHOD));
            } else {
                remoteActions.append(dbusAction("default",
                                                txt_qtn_msg_notification_reply,
                                                MESSAGING_SERVICE_NAME,
                                                OBJECT_PATH,
                                                MESSAGING_INTERFACE,
                                                START_CONVERSATION_METHOD,
                                                QVariantList() << pn->account()
                                                               << pn->targetId()
                                                               << static_cast<uint>(pn->chatType())));
            }

            remoteActions.append(dbusAction("app",
                                            QString(),
                                            MESSAGING_SERVICE_NAME,
                                            OBJECT_PATH,
                                            MESSAGING_INTERFACE,
                                            SHOW_INBOX_METHOD));
            break;

        case PersonalNotification::Voice:

            remoteActions.append(dbusAction("default",
                                            txt_qtn_call_notification_show_call_history,
                                            CALL_HISTORY_SERVICE_NAME,
                                            CALL_HISTORY_OBJECT_PATH,
                                            CALL_HISTORY_INTERFACE,
                                            CALL_HISTORY_METHOD,
                                            QVariantList() << CALL_HISTORY_PARAMETER));
            remoteActions.append(dbusAction("app",
                                            QString(),
                                            CALL_HISTORY_SERVICE_NAME,
                                            CALL_HISTORY_OBJECT_PATH,
                                            CALL_HISTORY_INTERFACE,
                                            CALL_HISTORY_METHOD,
                                            QVariantList() << CALL_HISTORY_PARAMETER));
            break;

        case PersonalNotification::Voicemail:

            remoteActions.append(dbusAction("default",
                                            txt_qtn_voicemail_notification_show_voicemail,
                                            CALL_HISTORY_SERVICE_NAME,
                                            VOICEMAIL_OBJECT_PATH,
                                            VOICEMAIL_INTERFACE,
                                            VOICEMAIL_METHOD));
            remoteActions.append(dbusAction("app",
                                            QString(),
                                            CALL_HISTORY_SERVICE_NAME,
                                            VOICEMAIL_OBJECT_PATH,
                                            VOICEMAIL_INTERFACE,
                                            VOICEMAIL_METHOD));
            break;
    }

    notification->setRemoteActions(remoteActions);
}

void NotificationManager::slotContactResolveFinished()
{
    DEBUG() << Q_FUNC_INFO;

    // All events are now resolved
    foreach (PersonalNotification *notification, m_unresolvedNotifications) {
        DEBUG() << "Resolved contact for notification" << notification->account() << notification->remoteUid() << notification->contactId();
        notification->updateRecipientData();
        addNotification(notification);
    }

    m_unresolvedNotifications.clear();
}

void NotificationManager::slotContactChanged(const RecipientList &recipients)
{
    DEBUG() << Q_FUNC_INFO << recipients;

    // Check all existing notifications and update if necessary
    foreach (NotificationGroup *group, m_Groups) {
        foreach (PersonalNotification *notification, group->notifications()) {
            if (recipients.contains(notification->recipient())) {
                DEBUG() << "Contact changed for notification" << notification->account() << notification->remoteUid() << notification->contactId();
                notification->updateRecipientData();
            }
        }
    }
}

void NotificationManager::slotContactInfoChanged(const RecipientList &recipients)
{
    DEBUG() << Q_FUNC_INFO << recipients;

    // Check all existing notifications and update if necessary
    foreach (NotificationGroup *group, m_Groups) {
        foreach (PersonalNotification *notification, group->notifications()) {
            if (recipients.contains(notification->recipient())) {
                DEBUG() << "Contact info changed for notification" << notification->account() << notification->remoteUid() << notification->contactId();
                notification->updateRecipientData();
            }
        }
    }
}

void NotificationManager::slotClassZeroError(const QDBusError &error)
{
    qWarning() << "Class 0 SMS notification failed:" << error.message();
}

CommHistory::GroupModel* NotificationManager::groupModel()
{
    if (!m_GroupModel) {
        m_GroupModel = new CommHistory::GroupModel(this);
        m_GroupModel->setResolveContacts(GroupManager::DoNotResolve);
        connect(m_GroupModel,
                SIGNAL(rowsAboutToBeRemoved(const QModelIndex&, int, int)),
                this,
                SLOT(slotGroupRemoved(const QModelIndex&, int, int)));
        connect(m_GroupModel,
                SIGNAL(dataChanged(const QModelIndex&, const QModelIndex&)),
                this,
                SLOT(slotGroupDataChanged(const QModelIndex&, const QModelIndex&)));
        if (!m_GroupModel->getGroups()) {
            qCritical() << "Failed to request group ";
            delete m_GroupModel;
            m_GroupModel = 0;
        }
    }

    return m_GroupModel;
}

void NotificationManager::slotGroupRemoved(const QModelIndex &index, int start, int end)
{
    DEBUG() << Q_FUNC_INFO;
    for (int i = start; i <= end; i++) {
        QModelIndex row = m_GroupModel->index(i, 0, index);
        Group group = m_GroupModel->group(row);
        if (group.isValid() && !group.recipients().isEmpty()) {
            removeConversationNotifications(group.recipients().value(0), group.chatType());
        }
    }
}
void NotificationManager::showVoicemailNotification(int count)
{
    Q_UNUSED(count)
    qWarning() << Q_FUNC_INFO << "Stub";
}

void NotificationManager::slotGroupDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    DEBUG() << Q_FUNC_INFO;

    QSet<NotificationGroup> updatedGroups;

    // Update MUC notifications if MUC topic has changed
    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
        QModelIndex row = m_GroupModel->index(i, 0);
        CommHistory::Group group = m_GroupModel->group(row);
        if (group.isValid()) {
            const Recipient &groupRecipient(group.recipients().value(0));

            foreach (NotificationGroup *g, m_Groups) {
                if (g->localUid() != groupRecipient.localUid()) {
                    continue;
                }

                foreach (PersonalNotification *pn, g->notifications()) {
                    // If notification is for MUC and matches to changed group...
                    if (!pn->chatName().isEmpty()) {
                        const Recipient notificationRecipient(pn->account(), pn->targetId());
                        if (notificationRecipient.matches(groupRecipient)) {
                            QString newChatName;
                            if (group.chatName().isEmpty() && pn->chatName() != txt_qtn_msg_group_chat)
                                newChatName = txt_qtn_msg_group_chat;
                            else if (group.chatName() != pn->chatName())
                                newChatName = group.chatName();

                            if (!newChatName.isEmpty()) {
                                DEBUG() << Q_FUNC_INFO << "Changing chat name to" << newChatName;
                                pn->setChatName(newChatName);
                            }
                        }
                    }
                }
            }
        }
    }
}

void NotificationManager::slotNgfEventFinished(quint32 id)
{
    if (id == m_ngfEvent)
        m_ngfEvent = 0;
}

void NotificationManager::slotVoicemailWaitingChanged()
{
    QOfonoMessageWaiting *mw = (QOfonoMessageWaiting*)sender();
    const bool waiting(mw->voicemailWaiting());
    const int messageCount(mw->voicemailMessageCount());

    DEBUG() << Q_FUNC_INFO << waiting << messageCount;

    uint currentId = 0;

    // See if there is a current notification for voicemail waiting
    QList<QObject*> notifications = Notification::notifications();
    foreach (QObject *o, notifications) {
        Notification *n = static_cast<Notification*>(o);
        if (n->category() == voicemailWaitingCategory) {
            if (waiting) {
                // The notification is already present; do nothing
                currentId = n->replacesId();
                DEBUG() << "Extant voicemail waiting notification:" << n->replacesId();
            } else {
                // Close this notification
                DEBUG() << "Closing voicemail waiting notification:" << n->replacesId();
                n->close();
            }
        }
    }
    qDeleteAll(notifications);
    notifications.clear();

    if (waiting) {
        const QString voicemailNumber(mw->voicemailMailboxNumber());

        // If ofono reports zero voicemail messages, we don't know the real number; report 1 as a fallback
        const int voicemailCount(messageCount > 0 ? messageCount : 1);

        // Publish a new voicemail-waiting notification
        Notification voicemailNotification;

        voicemailNotification.setAppName(NotificationGroup::groupName(PersonalNotification::Voicemail));
        voicemailNotification.setCategory(voicemailWaitingCategory);

        // If ofono reports zero voicemail messages, we don't know the real number; report 1 as a fallback
        voicemailNotification.setPreviewSummary(txt_qtn_call_voicemail_notification(voicemailCount));
        voicemailNotification.setPreviewBody(txt_qtn_voicemail_prompt);

        voicemailNotification.setSummary(voicemailNotification.previewSummary());
        voicemailNotification.setBody(voicemailNotification.previewBody());

        voicemailNotification.setItemCount(voicemailCount);

        QString displayName;
        QString service;
        QString path;
        QString iface;
        QString method;
        QVariantList args;
        if (!voicemailNumber.isEmpty()) {
            displayName = txt_qtn_voicemail_notification_call;
            service = VOICEMAIL_WAITING_SERVICE;
            path = VOICEMAIL_WAITING_OBJECT_PATH;
            iface = VOICEMAIL_WAITING_INTERFACE;
            method = VOICEMAIL_WAITING_METHOD;
            args.append(QVariant(QVariantList() << QString(QStringLiteral("tel://")) + voicemailNumber));
        } else {
            displayName = txt_qtn_call_notification_show_call_history;
            service = CALL_HISTORY_SERVICE_NAME;
            path = CALL_HISTORY_OBJECT_PATH;
            iface = CALL_HISTORY_INTERFACE;
            method = CALL_HISTORY_METHOD;
            args.append(CALL_HISTORY_PARAMETER);
        }

        voicemailNotification.setRemoteActions(QVariantList() << dbusAction("default", displayName, service, path, iface, method, args)
                                                              << dbusAction("app", QString(), service, path, iface, method, args));

        voicemailNotification.setReplacesId(currentId);
        voicemailNotification.publish();
        DEBUG() << (currentId ? "Updated" : "Created") << "voicemail waiting notification:" << voicemailNotification.replacesId();
    }
}

void NotificationManager::slotModemsChanged(QStringList modems)
{
    DEBUG() << "NotificationManager::slotModemsChanged";
    qDeleteAll(interfaces.values());
    interfaces.clear();
    foreach (QString path, modems)
        addModem(path);
}

void NotificationManager::slotModemAdded(QString path)
{
    DEBUG() << "NotificationManager::slotModemAdded: " << path;
    delete interfaces.take(path);
    addModem(path);
}

void NotificationManager::slotModemRemoved(QString path)
{
    DEBUG() << "NotificationManager::slotModemRemoved: " << path;
    delete interfaces.take(path);
}

void NotificationManager::slotValidChanged(bool valid)
{
    DEBUG() << "NotificationManager::slotValidChanged to: " << valid;
    QOfonoMessageWaiting *mw = (QOfonoMessageWaiting*)sender();
    if (mw->isValid()) {
        slotVoicemailWaitingChanged();
    }
}
