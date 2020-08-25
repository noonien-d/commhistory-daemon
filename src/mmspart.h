/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2014-2020 Jolla Ltd.
** Copyright (C) 2020 Open Mobile Platform LLC.
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
******************************************************************************/

#ifndef MMSPART_H
#define MMSPART_H

#include <QtDBus>
#include <QString>

struct MmsPart {
    QString fileName;
    QString contentType;
    QString contentId;
};

class MmsPartFd {
public:
    QFile file;
    QString fileName;
    QString contentType;
    QString contentId;

public:
    MmsPartFd() {}
    MmsPartFd(const QString path, const QString ct, const QString cid);
    MmsPartFd(const MmsPartFd &that);
    MmsPartFd &operator=(const MmsPartFd &that);
};

QDBusArgument &operator<<(QDBusArgument &arg, const MmsPart &part);
QDBusArgument &operator<<(QDBusArgument &arg, const MmsPartFd &part);

const QDBusArgument &operator>>(const QDBusArgument &arg, MmsPart &part);
const QDBusArgument &operator>>(const QDBusArgument &arg, MmsPartFd &part);

typedef QList<MmsPart> MmsPartList;
typedef QList<MmsPartFd> MmsPartFdList;

Q_DECLARE_METATYPE(MmsPart);
Q_DECLARE_METATYPE(MmsPartFd);
Q_DECLARE_METATYPE(MmsPartList);
Q_DECLARE_METATYPE(MmsPartFdList);

#endif // MMSPART_H
