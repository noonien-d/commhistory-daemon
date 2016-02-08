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

#include "smartmessaging.h"
#include "notificationmanager.h"
#include "constants.h"

#include <CommHistory/event.h>
#include <CommHistory/messagepart.h>

#define AGENT_PATH          "/commhistoryd/SmartMessagingAgent"
#define AGENT_SERVICE       "org.ofono.SmartMessagingAgent"

#define VCARD_CONTENT_TYPE  "text/x-vcard"
#define VCARD_EXTENSION     "vcf"

#define DEBUG_(x) qDebug() << "SmartMessaging:" << x

using namespace CommHistory;
using namespace RTComLogger;

SmartMessaging::SmartMessaging(QObject* parent) :
    MessageHandlerBase(parent, AGENT_PATH, AGENT_SERVICE)
{
    ofono = new QOfonoManager(this);
    connect(ofono, SIGNAL(modemAdded(QString)), this, SLOT(onModemAdded(QString)));
    connect(ofono, SIGNAL(modemRemoved(QString)), this, SLOT(onModemRemoved(QString)));
    DEBUG_("created");

    if (ofono->available()) {
        addAllModems();
    }
    connect(ofono, SIGNAL(availableChanged(bool)),
            this, SLOT(onOfonoAvailableChanged(bool)));
}

SmartMessaging::~SmartMessaging()
{
    onOfonoAvailableChanged(false);
}

void SmartMessaging::onOfonoAvailableChanged(bool available)
{
    DEBUG_("ofono available changed to" << available);
    if (available) {
        addAllModems();
    } else {
        qDeleteAll(interfaces.values());
        qDeleteAll(agents.values());
        interfaces.clear();
        agents.clear();
        agentToModemPaths.clear();
    }
}

void SmartMessaging::setup(const QString &path)
{
    if (path.isEmpty()) {
        qWarning() << "Empty modem path.";
        return;
    }

    QOfonoSmartMessagingAgent *agent = 0;
    QOfonoSmartMessaging *sm = 0;
    QString agentPath = agentPathFromModem(path);

    sm = interfaces[path];
    agent = agents[agentPath];

    DEBUG_("setup: registering agent" << agentPath << "for" << path);
    sm->registerAgent(agentPath);
    agent->setAgentPath(agentPath);
    connect(agent, SIGNAL(receiveBusinessCard(const QByteArray&, const QVariantMap&)),
            this, SLOT(onReceiveBusinessCard(const QByteArray&, const QVariantMap&)));
    connect(agent, SIGNAL(receiveAppointment(const QByteArray&, const QVariantMap&)),
            this, SLOT(onReceiveAppointment(const QByteArray&, const QVariantMap&)));
    connect(agent, SIGNAL(release()),
            this, SLOT(onRelease()));
}

void SmartMessaging::addAllModems()
{
    QStringList modems = ofono->modems();
    foreach (QString path, modems) {
        DEBUG_("modem" << path);
        addModem(path);
    }
}

void SmartMessaging::addModem(QString path)
{
    if (interfaces.contains(path))
        return;

    QOfonoSmartMessaging* sm = new QOfonoSmartMessaging(this);
    QString agentPath = agentPathFromModem(path);
    sm->setModemPath(path);
    interfaces.insert(path, sm);
    QOfonoSmartMessagingAgent *agent = new QOfonoSmartMessagingAgent(this);
    agents.insert(agentPath, agent);

    agentToModemPaths.insert(agentPath, path);

    if (sm->isValid()) {
        setup(path);
    }
    connect(sm, SIGNAL(validChanged(bool)), this, SLOT(onValidChanged(bool)));
}

void SmartMessaging::onModemAdded(QString path)
{
    DEBUG_("onModemAdded" << path);
    delete interfaces.take(path);
    addModem(path);
}

void SmartMessaging::onModemRemoved(QString path)
{
    DEBUG_("onModemRemoved" << path);
    QString agentPath = agentPathFromModem(path);
    agentToModemPaths.remove(agentPath);
    delete interfaces.take(path);
    delete agents.take(agentPath);
}

QString SmartMessaging::agentPathFromModem(const QString &modemPath)
{
    return AGENT_PATH + modemPath;
}

QString SmartMessaging::accountPath(const QString &modemPath)
{
    return RING_ACCOUNT_PATH_PREFIX + modemPath;
}

void SmartMessaging::onValidChanged(bool valid)
{
    QOfonoSmartMessaging* sm = (QOfonoSmartMessaging*)sender();
    if (valid) {
        setup(sm->modemPath());
    } else {
        DEBUG_("no agent for " << sm->modemPath());
    }
}

void SmartMessaging::onReceiveAppointment(const QByteArray &vcard, const QVariantMap &info)
{
    DEBUG_("ReceiveAppointment");
}

void SmartMessaging::onReceiveBusinessCard(const QByteArray &vcard, const QVariantMap &info)
{
    QOfonoSmartMessagingAgent* agent = (QOfonoSmartMessagingAgent*) sender();

    QString from = info.value("Sender").toString();
    DEBUG_("onReceiveBusinessCard to" << agent->agentPath() << ":" << vcard.length() << "bytes from" << from);
    if (vcard.isEmpty()) {
        qWarning() << "Empty vcard";
        return;
    }

    QString path = agentToModemPaths.value(agent->agentPath());
    QString ringAccountPath = accountPath(path);

    Event event;
    event.setType(Event::SMSEvent);
    event.setStartTime(QDateTime::currentDateTime());
    event.setEndTime(event.startTime());
    event.setDirection(Event::Inbound);
    event.setLocalUid(ringAccountPath);
    event.setRecipients(Recipient(ringAccountPath, from));
    event.setStatus(Event::DownloadingStatus);
    if (!setGroupForEvent(event)) {
        qCritical() << "Failed to handle group for vCard notification event; message dropped:" << event.toString();
        return;
    }

    EventModel model;
    if (!model.addEvent(event)) {
        qCritical() << "Failed to save vCard notification event; message dropped" << event.toString();
        return;
    }

    MessagePart part;
    if (!save(event.id(), vcard, part)) {
        qWarning() << "Failed to store vCard";
        model.deleteEvent(event.id());
        return;
    }

    event.setStatus(Event::ReceivedStatus);
    event.setMessageParts(QList<MessagePart>() << part);
    if (!model.modifyEvent(event)) {
        qCritical() << "Failed to update vCard event:" << event.toString();
        model.deleteEvent(event.id());
    }

    NotificationManager::instance()->showNotification(event, from, Group::ChatTypeP2P);
}

void SmartMessaging::onRelease()
{
    DEBUG_("Release");
}

bool SmartMessaging::save(int id, QByteArray vcard, MessagePart& part)
{
    bool ok = false;
    if (vcard.size()) {
        QString contentId("card." VCARD_EXTENSION);
        QString path = messagePartPath(id, contentId);
        if (!path.isEmpty()) {
            QFile file(path);
            if (file.open(QIODevice::WriteOnly)) {
                if (file.write(vcard) == vcard.size()) {
                    DEBUG_("Stored vCard to" << path);
                    part.setContentType(VCARD_CONTENT_TYPE);
                    part.setContentId(contentId);
                    part.setPath(path);
                    ok = true;
                }
                file.close();
            }
        }
    } else {
        qWarning () << "Empty vcard";
    }
    return ok;
}
