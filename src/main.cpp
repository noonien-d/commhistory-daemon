/******************************************************************************
**
** This file is part of commhistory-daemon.
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** Contact: Reto Zingg <reto.zingg@nokia.com>
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

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

#include <QLocale>

// Our includes
#include <TelepathyQt/Debug>
#include "logger.h"
#include "notificationmanager.h"
#include "commhistoryservice.h"
#include "commhistoryifadaptor.h"
#include "accountpresenceservice.h"
#include "accountpresenceifadaptor.h"
#include "messagereviver.h"
#include "contactauthorizationlistener.h"
#include "connectionutils.h"
#include "lastdialedcache.h"
#include "accountoperationsobserver.h"
#include "fscleanup.h"
#include "mmshandler.h"
#include "mmshandler_adaptor.h"
#include "smartmessaging.h"
#include "debug.h"

bool toggleDebug;

using namespace RTComLogger;

namespace {

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    if (!toggleDebug)
        return;

#ifndef QT_DEBUG
    if (!(type == QtCriticalMsg || type == QtFatalMsg))
        return;
#endif

    const char *logLevel = "";
    int priority = LOG_DEBUG;

    switch (type) {
    case QtDebugMsg:
        // don't use any prefix for debug messages
        priority = LOG_DEBUG;
        break;
    case QtWarningMsg:
        logLevel = "Warning: ";
        priority = LOG_WARNING;
        break;
    case QtCriticalMsg:
        logLevel = "CRITICAL: ";
        priority = LOG_CRIT;
        break;
    case QtFatalMsg:
        logLevel = "FATAL: ";
        priority = LOG_ALERT;
    }

    const QByteArray &msgData(message.toLocal8Bit());
    const char *msg = msgData.constData();

    QByteArray timestamp = QLocale::c().toString(QDateTime::currentDateTime(), "ss:zzz").toLocal8Bit();
    syslog(LOG_MAKEPRI(LOG_USER, priority),
           "%s [%s] %s",
           logLevel,
           timestamp.constData(),
           msg);

    if (type == QtFatalMsg)
        abort();
}

// handle SIGTERM to cleanup on exit
static int sigtermFd[2];

void termSignalHandler(int)
{
    char a = 1;
    if(write(sigtermFd[0], &a, sizeof(a)) < 1)
    {
        qWarning("Failed to handle term signal.");
    }
}

void setupSigtermHandler()
{
    struct sigaction term;

    term.sa_handler = termSignalHandler;
    sigemptyset(&term.sa_mask);
    term.sa_flags = SA_RESTART;

    if(::sigaction(SIGTERM, &term, 0))
        qFatal("Failed setup SIGTERM signal handler");
}

}

Q_DECL_EXPORT int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (argc > 1) {
        if (strcmp(argv[1],"-d") == 0) {
            toggleDebug = true;
        }
    }

    int logOption = LOG_NDELAY;

#ifdef QT_DEBUG
    if (app.arguments().contains(QLatin1String("-log-console")))
        logOption |= LOG_PERROR;
#endif

    openlog("COMMHISTORYD", logOption, 0);

    qInstallMessageHandler(messageHandler);

    DEBUG() << "MApplication created";

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd))
       qFatal("Couldn't create TERM socketpair");

    QSocketNotifier *snTerm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, &app);
    QObject::connect(snTerm, SIGNAL(activated(int)), &app, SLOT(quit()));
    setupSigtermHandler();

    QScopedPointer<QTranslator> engineeringEnglish(new QTranslator);
    engineeringEnglish->load("commhistoryd_eng_en", "/usr/share/translations");
    QScopedPointer<QTranslator> translator(new QTranslator);
    translator->load(QLocale(), "commhistoryd", "-", "/usr/share/translations");

    app.installTranslator(engineeringEnglish.data());
    app.installTranslator(translator.data());

    CommHistoryService *chService = CommHistoryService::instance();
    if (!chService->isRegistered()) {
        qCritical() << "CommHistoryService registration failed (already running or DBus not found), exiting";
        _exit(1);
    }
    new CommHistoryIfAdaptor(chService);
    DEBUG() << "CommHistoryService created";

    ConnectionUtils *utils = new ConnectionUtils(&app);

    // ContactAuthorizationListener needs to be updated with nemo-notifications and new UI handling
    //new ContactAuthorizationListener(utils, chService);

    AccountPresenceService *apService = new AccountPresenceService(utils->accountManager(), &app);
    if (!apService->isRegistered()) {
        qCritical() << "AccountPresenceService registration failed (already running or DBus not found), exiting";
        _exit(1);
    }
    new AccountPresenceIfAdaptor(apService);
    DEBUG() << "AccountPresenceService created";

    MessageReviver *reviver = new MessageReviver(utils, &app);
    DEBUG() << "Message reviver created, starting main loop";

    if (toggleDebug) {
        Tp::enableDebug(true);
        Tp::enableWarnings(true);
    }
    new Logger(utils->accountManager(),
               reviver,
               &app);
    DEBUG() << "Logger created";

    NotificationManager::instance();
    DEBUG() << "NotificationManager created";

    new LastDialedCache(&app);
    DEBUG() << "LastDialedCache created";

    // Init account operations observer to monitor account removals and to react to them.
    new AccountOperationsObserver(utils->accountManager(), &app);

    new MmsHandlerAdaptor(new MmsHandler(&app));
    new SmartMessaging(&app);
    new FsCleanup(&app);

    int result = app.exec();

    close(sigtermFd[0]);
    close(sigtermFd[1]);

    DEBUG() << "exit";

    return result;
}
