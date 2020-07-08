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

include( ../common-project-config.pri )
include( ../common-vars.pri )

# -----------------------------------------------------------------------------
# target setup
# -----------------------------------------------------------------------------
TARGET = commhistoryd

# -----------------------------------------------------------------------------
# dependencies
# -----------------------------------------------------------------------------
QT += dbus contacts versit
QT -= gui

PKGCONFIG += ngf-qt5 mce nemonotifications-qt5
PKGCONFIG += TelepathyQt5 commhistory-qt5 mlite5 mlocale5
PKGCONFIG += qofono-qt5 qofonoext contactcache-qt5 qtcontacts-sqlite-qt5-extensions
# clock_gettime
LIBS += -lrt

packagesExist(qt5-boostable) {
    DEFINES += HAS_BOOSTER
    PKGCONFIG += qt5-boostable
} else {
    warning("qt5-boostable not available; startup times will be slower")
}

# -----------------------------------------------------------------------------
# input
# -----------------------------------------------------------------------------
HEADERS += logger.h \
           channellistener.h \
           textchannellistener.h \
           streamchannellistener.h \
           loggerclientobserver.h \
           notificationmanager.h \
           serialisable.h \
           notificationgroup.h \
           personalnotification.h \
           commhistoryifadaptor.h \
           commhistoryservice.h \
           locstrings.h \
           messagereviver.h \
           connectionutils.h \
           contactauthorizationlistener.h \
           contactauthorizer.h \
           constants.h \
           accountoperationsobserver.h \
           accountpresenceifadaptor.h \
           accountpresenceservice.h \
           lastdialedcache.h \
           debug.h \
           fscleanup.h \
           mmshandler.h \
           mmspart.h \
           messagehandlerbase.h \
           smartmessaging.h

SOURCES += main.cpp \
           logger.cpp \
           channellistener.cpp \
           textchannellistener.cpp \
           streamchannellistener.cpp \
           loggerclientobserver.cpp \
           notificationmanager.cpp \
           serialisable.cpp \
           notificationgroup.cpp \
           personalnotification.cpp \
           commhistoryifadaptor.cpp \
           commhistoryservice.cpp \
           messagereviver.cpp \
           connectionutils.cpp \
           contactauthorizationlistener.cpp \
           contactauthorizer.cpp \
           accountoperationsobserver.cpp \
           accountpresenceifadaptor.cpp \
           accountpresenceservice.cpp \
           lastdialedcache.cpp \
           fscleanup.cpp \
           mmshandler.cpp \
           messagehandlerbase.cpp \
           smartmessaging.cpp

DBUS_ADAPTORS += mmshandler
mmshandler.files = org.nemomobile.MmsHandler.xml
mmshandler.header_flags = -i mmspart.h -i mmshandler.h -l MmsHandler
mmshandler.source_flags = -l MmsHandler

# -----------------------------------------------------------------------------
# Telepathy extensions.
# -----------------------------------------------------------------------------
EXTENSIONS_DIR = $$IN_PWD/TpExtensions
EXTENSIONS_GENERATED_DIR = $$EXTENSIONS_DIR/.gen
TOOLS_DIR = $$IN_PWD/../tools
PYTHON = python3
MOC = $$TOOLS_DIR/mocwrapper.sh

XINCLUDATOR = $$TOOLS_DIR/xincludator.py
CONSTANTS_GEN = $$TOOLS_DIR/qt-constants-gen.py
CLIENT_GEN = $$TOOLS_DIR/qt-client-gen.py

HEADERS += TpExtensions/connection.h \
           TpExtensions/Connection

SPEC_MAIN_FILES = $$EXTENSIONS_DIR/all.xml \
                  $$EXTENSIONS_DIR/connection.xml
spec_xml.name = Generating the Telepathy spec
spec_xml.input = SPEC_MAIN_FILES
spec_xml.output = $$EXTENSIONS_GENERATED_DIR/${QMAKE_FILE_IN_BASE}.xml
spec_xml.depends = $$EXTENSIONS_DIR/*.xml \
                   $$XINCLUDATOR
spec_xml.commands = \
        $(MKDIR) $$EXTENSIONS_GENERATED_DIR; \
        $$PYTHON $$XINCLUDATOR \
            ${QMAKE_FILE_IN} > ${QMAKE_FILE_OUT}.tmp && \
        mv ${QMAKE_FILE_OUT}.tmp ${QMAKE_FILE_OUT}
spec_xml.CONFIG += target_predeps no_link
QMAKE_EXTRA_COMPILERS += spec_xml

GENERATED_ALL_XML = $$EXTENSIONS_GENERATED_DIR/all.xml
constants_h.name = Generating the header for constants
constants_h.input = GENERATED_ALL_XML
constants_h.output = $$EXTENSIONS_GENERATED_DIR/constants.h
constants_h.variable_out = HEADERS
constants_h.depends = $$CONSTANTS_GEN
constants_h.commands = \
        $(MKDIR) $$EXTENSIONS_GENERATED_DIR; \
        $$PYTHON $$CONSTANTS_GEN \
            --namespace='CommHistoryTp' \
            --str-constant-prefix='COMM_HISTORY_TP_' \
            --specxml=${QMAKE_FILE_IN} \
            > ${QMAKE_FILE_OUT}.tmp && \
        mv ${QMAKE_FILE_OUT}.tmp ${QMAKE_FILE_OUT}
constants_h.CONFIG += target_predeps
QMAKE_EXTRA_COMPILERS += constants_h

GENERATED_CONNECTION_XML = $$EXTENSIONS_GENERATED_DIR/connection.xml
CLI_CONNECTION_STAMP_FILE = $$EXTENSIONS_GENERATED_DIR/cli-connection.stamp
cli_connection_stamp.name = Generating client files to the Connection interface
cli_connection_stamp.input = GENERATED_CONNECTION_XML
cli_connection_stamp.output = $$CLI_CONNECTION_STAMP_FILE
cli_connection_stamp.depends = $$CLIENT_GEN
cli_connection_stamp.commands = \
        $(MKDIR) $$EXTENSIONS_GENERATED_DIR; \
        $$PYTHON $$CLIENT_GEN \
            --namespace='CommHistoryTp::Client' \
            --typesnamespace='CommHistoryTp' \
            --headerfile=$$EXTENSIONS_GENERATED_DIR/cli-connection.h \
            --implfile=$$EXTENSIONS_GENERATED_DIR/cli-connection.cpp \
            --realinclude='$$EXTENSIONS_DIR/connection.h' \
            --specxml=$$GENERATED_ALL_XML \
            --ifacexml=$$GENERATED_ALL_XML \
            --extraincludes='\<TpExtensions/Connection\>' \
            --extraincludes='\<TelepathyQt/Connection\>' \
            --mainiface='Tp::Client::ConnectionInterface' && \
        $$MOC \$(DEFINES) \$(INCPATH) \
            $$EXTENSIONS_GENERATED_DIR/cli-connection.h \
            -o .moc/moc_cli-connection.cpp && \
        touch ${QMAKE_FILE_OUT}
cli_connection_stamp.CONFIG += no_link
QMAKE_EXTRA_COMPILERS += cli_connection_stamp

cli_connection_h.name = Generating ${QMAKE_FILE_OUT}
cli_connection_h.input = CLI_CONNECTION_STAMP_FILE
cli_connection_h.output = $$EXTENSIONS_GENERATED_DIR/cli-connection.h
cli_connection_h.variable_out = HEADERS
cli_connection_h.commands = true
QMAKE_EXTRA_COMPILERS += cli_connection_h

cli_connection_cpp.name = Generating ${QMAKE_FILE_OUT}
cli_connection_cpp.input = CLI_CONNECTION_STAMP_FILE
cli_connection_cpp.output = $$EXTENSIONS_GENERATED_DIR/cli-connection.cpp
cli_connection_cpp.variable_out = SOURCES
cli_connection_cpp.commands = true
QMAKE_EXTRA_COMPILERS += cli_connection_cpp

cli_connection_moc_cpp.name = Generating ${QMAKE_FILE_OUT}
cli_connection_moc_cpp.input = CLI_CONNECTION_STAMP_FILE
cli_connection_moc_cpp.output = .moc/moc_cli-connection.cpp
cli_connection_moc_cpp.variable_out = SOURCES
cli_connection_moc_cpp.commands = true
QMAKE_EXTRA_COMPILERS += cli_connection_moc_cpp

# -----------------------------------------------------------------------------
# common installation setup
# NOTE: remember to set headers.files before this include to have the headers
# properly setup.
# -----------------------------------------------------------------------------
include( ../common-installs-config.pri )
