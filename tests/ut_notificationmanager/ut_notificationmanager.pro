###############################################################################
#
# This file is part of commhistory-daemon.
#
# Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
# Contact: Reto Zingg <reto.zingg@nokia.com>
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License version 2.1 as
# published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this library; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
#
###############################################################################

#-----------------------------------------------------------------------------
# Project file for test ut_notificationmanager
#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
# common test configuration
#-----------------------------------------------------------------------------
!include(../tests.pri) : error( "Unable to include test.pri" )

#-----------------------------------------------------------------------------
# test specific configuration
#-----------------------------------------------------------------------------
TARGET = ut_notificationmanager

PKGCONFIG += mlocale5 TelepathyQt5 ngf-qt5 nemonotifications-qt5

TEST_SOURCES += $$COMMHISTORYDSRCDIR/notificationmanager.cpp \
                $$COMMHISTORYDSRCDIR/personalnotification.cpp \
                $$COMMHISTORYDSRCDIR/serialisable.cpp \
                $$COMMHISTORYDSRCDIR/commhistoryservice.cpp
TEST_HEADERS += $$COMMHISTORYDSRCDIR/notificationmanager.h \
                $$COMMHISTORYDSRCDIR/personalnotification.h \
                $$COMMHISTORYDSRCDIR/serialisable.h \
                $$COMMHISTORYDSRCDIR/commhistoryservice.h

HEADERS     += ut_notificationmanager.h \
            $$TEST_HEADERS

SOURCES     += ut_notificationmanager.cpp \
            $$TEST_SOURCES

DESTDIR = ../bin

# End of File

