/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2014-2015 Jolla Ltd.
** Contact: Slava Monich <slava.monich@jolla.com>
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

#ifndef MMSHANDLER_H
#define MMSHANDLER_H

#include "messagehandlerbase.h"
#include "mmspart.h"

namespace CommHistory {
    class MessagePart;
    class Event;
    class Group;
}

class QDBusPendingCallWatcher;
class ContextProperty;
class MGConfItem;
class MDConfGroup;

class MmsHandler : public MessageHandlerBase
{
    Q_OBJECT

public:
    explicit MmsHandler(QObject *parent);

public Q_SLOTS:
    QString messageNotification(const QString &imsi, const QString &from, const QString &subject,
            uint expiry, const QByteArray &data);
    void messageReceiveStateChanged(const QString &recId, int state);
    void messageReceived(const QString &recId, const QString &mmsId, const QString &from,
            const QStringList &to, const QStringList &cc, const QString &subj, uint date, int priority,
            const QString &cls, bool readReport, MmsPartList parts);

    void deliveryReport(const QString &imsi, const QString &mmsId, const QString &recipient, int status);
    void messageSendStateChanged(const QString &recId, int state, const QString &details);
    void messageSent(const QString &recId, const QString &mmsId);
    void readReport(const QString &imsi, const QString &mmsId, const QString &recipient, int status);
    void readReportSendStatus(const QString &recId, int status);

    int sendMessage(const QStringList &to, const QStringList &cc, const QStringList &bcc,
            const QString &subject, MmsPartList parts);
    void sendMessageFromEvent(int eventId);

private Q_SLOTS:
    void onSendMessageFinished(QDBusPendingCallWatcher *call);
    void onDataProhibitedChanged();
    void onSubscriberIdentityChanged();
    void onEventsUpdated(const QList<CommHistory::Event> &events);
    void onGroupsUpdatedFull(const QList<CommHistory::Group> &groups);

private:
    static QDBusPendingCall callEngine(const QString &method, const QVariantList &args);
    void eventMarkedAsRead(CommHistory::Event &event);

    void sendMessageFromEvent(CommHistory::Event &event);
    bool copyMmsPartFiles(const MmsPartList &parts, int eventId, QList<CommHistory::MessagePart> &eventParts, QString &freeText);
    QString copyMessagePartFile(const QString &sourcePath, int eventId, const QString &contentId);

    bool isDataProhibited();
    bool canSendReadReports();

private:
    ContextProperty *m_cellularStatusProperty;
    ContextProperty *m_roamingAllowedProperty;
    ContextProperty *m_subscriberIdentityProperty;
    QList<int> m_activeEvents;
    MDConfGroup *m_imsiSettings;
};

#endif // MMSHANDLER_H
