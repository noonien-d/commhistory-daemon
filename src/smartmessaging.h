/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2014 Jolla Ltd.
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

#ifndef SMARTMESSAGING_H
#define SMARTMESSAGING_H

#include "messagehandlerbase.h"
#include "qofonomanager.h"
#include "qofonosmartmessaging.h"
#include "qofonosmartmessagingagent.h"

namespace CommHistory {
    class MessagePart;
}

class SmartMessaging: public MessageHandlerBase
{
    Q_OBJECT

public:
    SmartMessaging(QObject* parent);
    ~SmartMessaging();

private Q_SLOTS:
    void onOfonoAvailableChanged(bool available);
    void onModemAdded(QString path);
    void onModemRemoved(QString path);
    void onValidChanged(bool valid);
    void onReceiveBusinessCard(const QByteArray &vcard, const QVariantMap &info);
    void onReceiveAppointment(const QByteArray &vcard, const QVariantMap &info);
    void onRelease();

private:
    QString agentPathFromModem(const QString &modemPath);
    QString accountPath(const QString &modemPath);
    void addAllModems();
    void addModem(QString path);
    void setup(const QString &path);

private:
    static bool save(int id, QByteArray vcard, CommHistory::MessagePart& part);

private:
    QOfonoManager *ofono;
    QHash<QString,QOfonoSmartMessaging*> interfaces;
    QHash<QString,QOfonoSmartMessagingAgent*> agents;
    QHash<QString,QString> agentToModemPaths;
};

#endif // SMARTMESSAGING_H
