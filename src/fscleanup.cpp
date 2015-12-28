/****************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2014 Jolla Ltd.
** Contact: Slava Monich <slava.monich@jolla.com>
**
** This library is free software; you can redistribute it and/or modify
** it under the terms of the GNU Lesser General Public License version 2.1
** as published by the Free Software Foundation.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** Lesser General Public License for more details.
**
** You should have received a copy of the GNU Lesser General Public
** License along with this library; if not, write to the Free Software
** Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
**
****************************************************************************/

#include "fscleanup.h"
#include "debug.h"

#include <CommHistory/commhistorydatabasepath.h>
#include <CommHistory/databaseio.h>
#include <CommHistory/constants.h>

#include <QDirIterator>
#include <QDBusConnection>

#define DEBUG_(x) qDebug() << "FsCleanup:" << x

FsCleanup::FsCleanup(QObject* aParent) :
    QObject(aParent)
{
    QDBusConnection dbus(QDBusConnection::sessionBus());
    dbus.connect(QString(), QString(), COMM_HISTORY_INTERFACE,
        EVENT_DELETED_SIGNAL, this, SLOT(onEventDeleted(int)));
    dbus.connect(QString(), QString(), COMM_HISTORY_INTERFACE,
        GROUPS_DELETED_SIGNAL, this, SLOT(onGroupsDeleted(QList<int>)));
    fullCleanup();
}

void FsCleanup::onEventDeleted(int aEventId)
{
    DEBUG_("Event" << aEventId << "deleted");
    CommHistory::DatabaseIO* io = CommHistory::DatabaseIO::instance();
    if (!io->eventExists(aEventId)) {
        deleteFiles(aEventId);
    } else {
        qWarning() << "Ignoring stray delete event for" << aEventId;
    }
}

void FsCleanup::onGroupsDeleted(QList<int> aGroupIds)
{
    DEBUG_(aGroupIds.count() << "group(s) deleted");
    fullCleanup();
}

void FsCleanup::fullCleanup()
{
    DEBUG_("Running full cleanup");
    CommHistory::DatabaseIO* io = CommHistory::DatabaseIO::instance();
    QDirIterator it(CommHistoryDatabasePath::dataDir(),
        QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        bool ok = false;
        int id = it.fileName().toInt(&ok);
        if (ok && !io->eventExists(id)) {
            deleteFiles(id);
        }
    }
    DEBUG_("Cleanup done");
}

void FsCleanup::deleteFiles(int aEventId)
{
    removeDir(CommHistoryDatabasePath::dataDir(aEventId));
}

bool FsCleanup::removeDir(QString aDirPath)
{
    bool result = true;
    QDir dir(aDirPath);
    if (dir.exists()) {
        DEBUG_("Removing" << aDirPath);
        QFileInfoList list = dir.entryInfoList(QDir::NoDotAndDotDot |
            QDir::System | QDir::Hidden  | QDir::AllDirs | QDir::Files,
            QDir::DirsFirst);
        for (int i=0; i<list.count(); i++) {
            QFileInfo info(list.at(i));
            QString path(info.absoluteFilePath());
            if (info.isDir()) {
                result = removeDir(path);
            } else {
                result = QFile::remove(path);
            }
            if (!result) {
                qWarning() << "Failed to remove" << path;
                return result;
            }
        }
        result = dir.rmdir(aDirPath);
        if (!result) {
            qWarning() << "Failed to remove" << aDirPath;
        }
    }
    return result;
}
