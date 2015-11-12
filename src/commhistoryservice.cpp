/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2015 Jolla Ltd.
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

#include <QtDBus>
#include <QCoreApplication>
#include "commhistoryservice.h"
#include "constants.h"

CommHistoryService *CommHistoryService::instance()
{
    static CommHistoryService *obj = 0;
    if (!obj)
        obj = new CommHistoryService(qApp);
    return obj;
}

CommHistoryService::CommHistoryService( QObject* parent )
    : QObject(parent),
      m_IsRegistered(false),
      m_callHistoryObserved(false),
      m_inboxObserved(false)
{
    if (!QDBusConnection::sessionBus().isConnected()) {
        qCritical() << "ERROR: No DBus session bus found!";
        return;
    }

    if (parent) {
        if(!QDBusConnection::sessionBus().registerObject(COMM_HISTORY_DAEMON_OBJECT_PATH, this)) {
            qWarning() << "Object registration failed!";
        } else {
            if(!QDBusConnection::sessionBus().registerService(COMM_HISTORY_DAEMON_SERVICE_NAME)) {
                qWarning() << "Unable to register commhistory service!"
                           << QDBusConnection::sessionBus().lastError();
            } else {
                m_IsRegistered = true;
            }
        }
    }
}

CommHistoryService::~CommHistoryService()
{
    QDBusConnection::sessionBus().unregisterObject(COMM_HISTORY_DAEMON_OBJECT_PATH);
    QDBusConnection::sessionBus().unregisterService(COMM_HISTORY_DAEMON_SERVICE_NAME);
}

void CommHistoryService::activateAuthorization(const QString& contactId,
                                               const QString& accountPath,
                                               const QString& filename,
                                               const QString& message,
                                               const QString& transactionId,
                                               const QString& accountUniqueIdentifier)
{
    Q_EMIT showAuthorizationDialog(contactId, accountPath, filename, message,
                                  transactionId, accountUniqueIdentifier);
}

void CommHistoryService::setCallHistoryObserved(bool observed)
{
    if (observed != m_callHistoryObserved) {
        m_callHistoryObserved = observed;
        emit callHistoryObservedChanged(observed);
    }
}

void CommHistoryService::setInboxObserved(bool observed, const QString &filterAccount)
{
    if (observed != m_inboxObserved || filterAccount != m_inboxFilterAccount) {
        m_inboxObserved = observed;
        m_inboxFilterAccount = filterAccount;
        emit inboxObservedChanged(observed, filterAccount);
    }
}

void CommHistoryService::setObservedConversations(const QVariantList &arg)
{
    QList<Conversation> conversations;
    foreach (const QVariant &v1, arg) {
        const QDBusArgument arg2 = v1.value<QDBusArgument>();
        arg2.beginArray();
        QVariantList values;
        while (!arg2.atEnd()) {
            QVariant v2;
            arg2 >> v2;
            values.append(v2);
        }
        arg2.endArray();

        if (values.count() == 3) {
            const QString localUid(values.at(0).toString());
            const QString remoteUid(values.at(1).toString());
            const int chatType(values.at(2).toInt());
            conversations.append(qMakePair(CommHistory::Recipient(localUid, remoteUid), chatType));
        }
    }

    m_observedConversations = conversations;
    emit observedConversationsChanged(m_observedConversations);
}

bool CommHistoryService::isRegistered()
{
    return m_IsRegistered;
}
