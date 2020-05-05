/*
 * This file is part of TelepathyQt4
 *
 * Copyright (C) 2008-2009 Collabora Ltd. <http://www.collabora.co.uk/>
 * Copyright (C) 2008-2009 Nokia Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "ReferencedHandles"

#include <QPointer>
#include <QSharedData>

namespace Tp
{

struct ReferencedHandles::Private : public QSharedData
{
    uint handleType;
    UIntList handles;

    Private()
    {
        handleType = 0;
    }

    Private(uint handleType,
            const UIntList &handles)
        : handleType(handleType), handles(handles)
    {
        Q_ASSERT(handleType != 0);

    }

    Private(const Private &a)
        : QSharedData(a),
          handleType(a.handleType),
          handles(a.handles)
    {
    }

    ~Private()
    {
    }

private:
    void operator=(const Private&);
};

/**
 * \class ReferencedHandles
 * \ingroup clientconn
 * \headerfile TelepathyQt/referenced-handles.h <TelepathyQt/ReferencedHandles>
 *
 * \brief Helper container for safe management of handle lifetimes. Every handle
 * in a ReferencedHandles container is guaranteed to be valid (and stay valid,
 * as long it's in at least one ReferencedHandles container).
 *
 * The class offers a QList-style API. However, from the mutable operations,
 * only the operations for which the validity guarantees can be preserved are
 * provided. This means no functions which can add an arbitrary handle to the
 * container are included - the only way to add handles to the container is to
 * reference them using Connection::referenceHandles() and appending the
 * resulting ReferenceHandles instance.
 *
 * ReferencedHandles is a implicitly shared class.
 */

ReferencedHandles::ReferencedHandles()
    : mPriv(new Private)
{
}

ReferencedHandles::ReferencedHandles(const ReferencedHandles &other)
    : mPriv(other.mPriv)
{
}

ReferencedHandles::~ReferencedHandles()
{
}

uint ReferencedHandles::handleType() const
{
    return mPriv->handleType;
}

uint ReferencedHandles::at(int i) const
{
    return mPriv->handles[i];
}

uint ReferencedHandles::value(int i, uint defaultValue) const
{
    return mPriv->handles.value(i, defaultValue);
}

ReferencedHandles::const_iterator ReferencedHandles::begin() const
{
    return mPriv->handles.begin();
}

ReferencedHandles::const_iterator ReferencedHandles::end() const
{
    return mPriv->handles.end();
}

bool ReferencedHandles::contains(uint handle) const
{
    return mPriv->handles.contains(handle);
}

int ReferencedHandles::count(uint handle) const
{
    return mPriv->handles.count(handle);
}

int ReferencedHandles::indexOf(uint handle, int from) const
{
    return mPriv->handles.indexOf(handle, from);
}

bool ReferencedHandles::isEmpty() const
{
    return mPriv->handles.isEmpty();
}

int ReferencedHandles::lastIndexOf(uint handle, int from) const
{
    return mPriv->handles.lastIndexOf(handle, from);
}

ReferencedHandles ReferencedHandles::mid(int pos, int length) const
{
    return ReferencedHandles(handleType(), mPriv->handles.mid(pos, length));
}

int ReferencedHandles::size() const
{
    return mPriv->handles.size();
}

void ReferencedHandles::clear()
{
    mPriv->handles.clear();
}

void ReferencedHandles::move(int from, int to)
{
    mPriv->handles.move(from, to);
}

int ReferencedHandles::removeAll(uint handle)
{
    int count = mPriv->handles.removeAll(handle);
    return count;
}

void ReferencedHandles::removeAt(int i)
{
    mPriv->handles.removeAt(i);
}

bool ReferencedHandles::removeOne(uint handle)
{
    bool wasThere = mPriv->handles.removeOne(handle);
    return wasThere;
}

void ReferencedHandles::swap(int i, int j)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
    mPriv->handles.swapItemsAt(i, j);
#else
    mPriv->handles.swap(i, j);
#endif
}

uint ReferencedHandles::takeAt(int i)
{
    return mPriv->handles.takeAt(i);
}

ReferencedHandles ReferencedHandles::operator+(const ReferencedHandles &another) const
{
    return ReferencedHandles(handleType(),
            mPriv->handles + another.mPriv->handles);
}

ReferencedHandles &ReferencedHandles::operator=(
        const ReferencedHandles &another)
{
    mPriv = another.mPriv;
    return *this;
}

bool ReferencedHandles::operator==(const ReferencedHandles &another) const
{
    return handleType() == another.handleType() &&
        mPriv->handles == another.mPriv->handles;
}

bool ReferencedHandles::operator==(const UIntList &list) const
{
    return mPriv->handles == list;
}

UIntList ReferencedHandles::toList() const
{
    return mPriv->handles;
}

ReferencedHandles::ReferencedHandles(uint handleType, const UIntList &handles)
    : mPriv(new Private(handleType, handles))
{
}

} // Tp
