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

#include "mmspart.h"
#include <unistd.h>

MmsPartFd::MmsPartFd(const QString path, const QString ct, const QString cid) :
    file(path),
    fileName(QFileInfo(path).fileName()),
    contentType(ct),
    contentId(cid)
{
    file.open(QIODevice::ReadOnly);
}

MmsPartFd::MmsPartFd(const MmsPartFd &that) :
    fileName(that.fileName),
    contentType(that.contentType),
    contentId(that.contentId)
{
    if (that.file.isOpen()) {
        file.open(dup(that.file.handle()), QIODevice::ReadOnly, QFileDevice::AutoCloseHandle);
    }
}

MmsPartFd &MmsPartFd::operator=(const MmsPartFd &that)
{
    fileName = that.fileName;
    contentType = that.contentType;
    contentId = that.contentId;
    file.close();
    if (that.file.isOpen()) {
        file.open(dup(that.file.handle()), QIODevice::ReadOnly, QFileDevice::AutoCloseHandle);
    }
    return *this;
}

QDBusArgument &operator<<(QDBusArgument &arg, const MmsPart &part)
{
    arg.beginStructure();
    arg << part.fileName << part.contentType << part.contentId;
    arg.endStructure();
    return arg;
}

QDBusArgument &operator<<(QDBusArgument &arg, const MmsPartFd &part)
{
    QDBusUnixFileDescriptor fd(part.file.handle());
    arg.beginStructure();
    arg << fd << part.fileName << part.contentType << part.contentId;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, MmsPart &part)
{
    arg.beginStructure();
    arg >> part.fileName >> part.contentType >> part.contentId;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, MmsPartFd &part)
{
    QDBusUnixFileDescriptor fd;
    arg.beginStructure();
    arg >> fd >> part.fileName >> part.contentType >> part.contentId;
    arg.endStructure();

    part.file.close();
    if (fd.isValid()) {
        part.file.open(dup(fd.fileDescriptor()), QIODevice::ReadOnly, QFileDevice::AutoCloseHandle);
    }
    return arg;
}
