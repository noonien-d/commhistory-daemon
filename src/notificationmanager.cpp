/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2020 Open Mobile Platform LLC.
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
static const QString voicemailWaitingCategory = "x-nemo.messaging.voicemail-waiting";

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
    qDeleteAll(m_notifications);
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

    foreach (PersonalNotification *pn, m_notifications) {
        if (pn->eventToken() == event.messageToken()) {
            pn->setNotificationText(text);
            return true;
        }
    }

    return false;
}

static PersonalNotification *findNotification(
        QList<PersonalNotification *> *notifications, const CommHistory::Event& event)
{
    const CommHistory::Recipient recipient = event.recipients().value(0);

    auto it = std::find_if(
                notifications->begin(),
                notifications->end(),
                [&](PersonalNotification *notification) {
        return notification->eventType() == event.type() && notification->recipient().matches(recipient);
    });

    return it != notifications->end() ? *it : nullptr;
}

static void amendCallNotification(
        PersonalNotification *personal, const CommHistory::Event& event, const QString &text)
{
    personal->setEventToken(event.messageToken());

    Notification *notification = personal->notification();

    notification->setItemCount(qMax(1, notification->itemCount()) + 1);
    notification->setTimestamp(QDateTime::currentDateTime());

    if (event.type() == CommHistory::Event::CallEvent) {
        personal->setNotificationText(txt_qtn_call_missed(notification->itemCount()));
    } else {
        personal->setNotificationText(text);
    }
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
    if (event.isValid() && updateEditedEvent(event, text)) {
        return;
    }

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

    if (event.type() == CommHistory::Event::CallEvent
            || event.type() == CommHistory::Event::VoicemailEvent) {
        if (PersonalNotification *personal = findNotification(&m_unresolvedNotifications, event)) {
            amendCallNotification(personal, event, text);

            return;
        } else if (PersonalNotification *personal = findNotification(&m_notifications, event)) {
            amendCallNotification(personal, event, text);

            if (event.type() == CommHistory::Event::CallEvent) {
                Notification *notification = personal->notification();

                notification->clearPreviewSummary();
                notification->clearPreviewBody();
            }

            personal->publishNotification();

            return;
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

static void deleteNotifications(
        QList<PersonalNotification *> *notifications, QList<PersonalNotification *>::iterator eraseFrom)
{
    if (eraseFrom != notifications->end()) {
        for (auto it = eraseFrom; it != notifications->end(); ++it) {
            PersonalNotification *notification = *it;
            notification->removeNotification();
            notification->deleteLater();
        }

        notifications->erase(eraseFrom, notifications->end());
    }
}

static void removeListNotifications(
        QList<PersonalNotification *> *notifications, const QString &accountPath, const QList<int> &removeTypes)
{
    auto eraseFrom = std::find_if(notifications->begin(), notifications->end(), [&](PersonalNotification *notification) {
        return notification->account() == accountPath && removeTypes.contains(notification->eventType());
    });

    deleteNotifications(notifications, eraseFrom);
}

void NotificationManager::removeNotifications(const QString &accountPath, const QList<int> &removeTypes)
{
    DEBUG() << Q_FUNC_INFO << "Removing notifications of account " << accountPath;

    // remove matched notifications
    removeListNotifications(&m_notifications, accountPath, removeTypes);
    removeListNotifications(&m_unresolvedNotifications, accountPath, removeTypes);
}

void NotificationManager::removeConversationNotifications(const CommHistory::Recipient &recipient,
                                                          CommHistory::Group::ChatType chatType)
{
    auto eraseFrom = std::find_if(m_notifications.begin(), m_notifications.end(), [&](PersonalNotification *notification) {
            return notification->collection() == PersonalNotification::Messaging
                    && notification->chatType() == chatType
                    && (chatType == CommHistory::Group::ChatTypeP2P
                        ? recipient.matches(notification->recipient())
                        : recipient.matches(Recipient(notification->account(), notification->targetId())));
    });

    deleteNotifications(&m_notifications, eraseFrom);
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

    auto eraseFrom = std::find_if(m_notifications.begin(), m_notifications.end(), [&](PersonalNotification *notification) {
        return types.contains(notification->eventType());
    });

    deleteNotifications(&m_notifications, eraseFrom);
}

void NotificationManager::addNotification(PersonalNotification *notification)
{
    if (!m_notifications.contains(notification)) {
        connect(notification, &PersonalNotification::hasPendingEventsChanged, this, [notification](bool hasEvents) {
            if (hasEvents) {
                notification->publishNotification();
            }
        });

        if (notification->hasPendingEvents()) {
            notification->publishNotification();
        }

        m_notifications.append(notification);
    }
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
                // Default action: show the inbox
                remoteActions.append(dbusAction("default",
                                                QString(),
                                                MESSAGING_SERVICE_NAME,
                                                OBJECT_PATH,
                                                MESSAGING_INTERFACE,
                                                SHOW_INBOX_METHOD));
            } else {
                // Default action: show the message
                remoteActions.append(dbusAction("default",
                                                QString(),
                                                MESSAGING_SERVICE_NAME,
                                                OBJECT_PATH,
                                                MESSAGING_INTERFACE,
                                                START_CONVERSATION_METHOD,
                                                QVariantList() << pn->account()
                                                               << pn->targetId()
                                                               << false));
            }

            if (pn->eventType() == CommHistory::Event::IMEvent
                    || pn->eventType() == CommHistory::Event::SMSEvent
                    || pn->eventType() == CommHistory::Event::MMSEvent) {

                if (pn->eventType() == CommHistory::Event::IMEvent || pn->hasPhoneNumber()) {
                    // Named action: "Reply"
                    remoteActions.append(dbusAction(QString(),
                                                    txt_qtn_msg_notification_reply,
                                                    MESSAGING_SERVICE_NAME,
                                                    OBJECT_PATH,
                                                    MESSAGING_INTERFACE,
                                                    START_CONVERSATION_METHOD,
                                                    QVariantList() << pn->account()
                                                                   << pn->targetId()
                                                                   << true));
                }
            }

            if (pn->eventType() == CommHistory::Event::SMSEvent
                    || pn->eventType() == CommHistory::Event::MMSEvent
                    || pn->eventType() == VOICEMAIL_SMS_EVENT_TYPE) {
                if (pn->hasPhoneNumber()) {
                    // Named action: "Call"
                    remoteActions.append(dbusAction(QString(),
                                                    txt_qtn_msg_notification_call,
                                                    VOICECALL_SERVICE,
                                                    VOICECALL_OBJECT_PATH,
                                                    VOICECALL_INTERFACE,
                                                    VOICECALL_DIAL_METHOD,
                                                    QVariantList() << pn->remoteUid()));
                }
            }

            break;

        case PersonalNotification::Voice:

            // Missed calls.
            // Default action: show Call History
            remoteActions.append(dbusAction("default",
                                            QString(),
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

            if (pn->hasPhoneNumber()) {
                remoteActions.append(dbusAction(QString(),
                                                txt_qtn_call_notification_call_back,
                                                VOICECALL_SERVICE,
                                                VOICECALL_OBJECT_PATH,
                                                VOICECALL_INTERFACE,
                                                VOICECALL_DIAL_METHOD,
                                                QVariantList() << pn->remoteUid()));

                remoteActions.append(dbusAction(QString(),
                                                txt_qtn_call_notification_send_message,
                                                MESSAGING_SERVICE_NAME,
                                                OBJECT_PATH,
                                                MESSAGING_INTERFACE,
                                                START_CONVERSATION_METHOD,
                                                QVariantList() << pn->account()
                                                << pn->targetId()
                                                << true));
            }

            break;

        case PersonalNotification::Voicemail:

            // Default action: show voicemail
            remoteActions.append(dbusAction("default",
                                            QString(),
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
    foreach (PersonalNotification *notification, m_notifications) {
        if (recipients.contains(notification->recipient())) {
            DEBUG() << "Contact changed for notification" << notification->account() << notification->remoteUid() << notification->contactId();
            notification->updateRecipientData();
        }
    }
}

void NotificationManager::slotContactInfoChanged(const RecipientList &recipients)
{
    DEBUG() << Q_FUNC_INFO << recipients;

    // Check all existing notifications and update if necessary
    foreach (PersonalNotification *notification, m_notifications) {
        if (recipients.contains(notification->recipient())) {
            DEBUG() << "Contact info changed for notification" << notification->account() << notification->remoteUid() << notification->contactId();
            notification->updateRecipientData();
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

    // Update MUC notifications if MUC topic has changed
    for (int i = topLeft.row(); i <= bottomRight.row(); i++) {
        QModelIndex row = m_GroupModel->index(i, 0);
        CommHistory::Group group = m_GroupModel->group(row);
        if (group.isValid()) {
            const Recipient &groupRecipient(group.recipients().value(0));

            foreach (PersonalNotification *pn, m_notifications) {
                // If notification is for MUC and matches to changed group...
                if (pn->account() == groupRecipient.localUid() && !pn->chatName().isEmpty()) {
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

        voicemailNotification.setAppName(txt_qtn_msg_voicemail_group);
        voicemailNotification.setCategory(voicemailWaitingCategory);

        // If ofono reports zero voicemail messages, we don't know the real number; report 1 as a fallback
        voicemailNotification.setPreviewSummary(txt_qtn_call_voicemail_notification(voicemailCount));
        voicemailNotification.setPreviewBody(txt_qtn_voicemail_prompt);

        voicemailNotification.setSummary(voicemailNotification.previewSummary());

        voicemailNotification.setItemCount(voicemailCount);

        QString service;
        QString path;
        QString iface;
        QString method;
        QVariantList args;
        if (!voicemailNumber.isEmpty()) {
            service = VOICECALL_SERVICE;
            path = VOICECALL_OBJECT_PATH;
            iface = VOICECALL_INTERFACE;
            method = VOICECALL_DIAL_METHOD;
            args.append(QVariant(QVariantList() << QString(QStringLiteral("tel://")) + voicemailNumber));
        } else {
            service = CALL_HISTORY_SERVICE_NAME;
            path = CALL_HISTORY_OBJECT_PATH;
            iface = CALL_HISTORY_INTERFACE;
            method = CALL_HISTORY_METHOD;
            args.append(CALL_HISTORY_PARAMETER);
        }

        voicemailNotification.setRemoteActions(QVariantList() << dbusAction("default", QString(), service, path, iface, method, args)
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
