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

#include "messagehandlerbase.h"
#include "constants.h"
#include "debug.h"

#include <CommHistory/event.h>
#include <CommHistory/groupmanager.h>
#include <CommHistory/commhistorydatabasepath.h>

#include <QDBusConnection>
#include <QDBusError>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDir>

using namespace CommHistory;

MessageHandlerBase::MessageHandlerBase(QObject* parent, QString objectPath,
    QString serviceName) :
    QObject(parent),
    m_isRegistered(false),
    groupManager(NULL)
{
    QDBusConnection dbus = QDBusConnection::systemBus();
    if (!dbus.isConnected()) {
        qCritical() << "ERROR: No DBus system bus found!";
    } else if (!dbus.registerObject(objectPath, this)) {
        qWarning() << "Object registration failed:" << dbus.lastError();;
    } else if (!dbus.registerService(serviceName)) {
        qWarning() << "Service registration failed:" << dbus.lastError();
    } else {
        m_isRegistered = true;
    }
}

QString MessageHandlerBase::sanitizeName(QString name)
{
    return name.replace(QRegularExpression("[^-.0-9a-zA-Z]"), "_");
}

QString MessageHandlerBase::messagePartPath(int eventId, QString contentId)
{
    QDir dataDir(CommHistoryDatabasePath::dataDir(eventId));
    if (dataDir.exists() || dataDir.mkpath(".")) {
        return dataDir.filePath(sanitizeName(contentId));
    } else {
        qCritical() << "Cannot create directory for MMS message parts:" << dataDir.path();
        return QString();
    }
}

bool MessageHandlerBase::setGroupForEvent(Event& event)
{
    if (!groupManager) {
        groupManager = new GroupManager(this);
        if (!groupManager->getGroups()) {
            delete groupManager;
            groupManager = NULL;
            return false;
        }
    }

    GroupObject* group = groupManager->findGroup(event.localUid(), event.recipients().value(0).remoteUid());
    if (group) {
        event.setGroupId(group->id());
        return true;
    }

    DEBUG() << "Creating new group for event" << event.recipients().value(0).remoteUid();
    Group newGroup;
    newGroup.setLocalUid(event.localUid());
    newGroup.setRecipients(Recipient(event.localUid(), event.recipients().value(0).remoteUid()));
    if (!groupManager->addGroup(newGroup)) {
        qCritical() << "Failed adding new group for event" << newGroup.toString();
        return false;
    }

    event.setGroupId(newGroup.id());
    return true;
}
