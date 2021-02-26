#include <CommHistory/GroupModel>
#include <QCoreApplication>

#include "notificationmanager.h"

using namespace RTComLogger;

NotificationManager* NotificationManager::m_pInstance = 0;

NotificationManager::NotificationManager(QObject *parent) :
    QObject(parent)
{
    // Temporary override until qtpim supports QTCONTACTS_MANAGER_OVERRIDE
    m_pContactManager = new QContactManager(QString::fromLatin1("org.nemomobile.contacts.sqlite"));
    m_GroupModel = new CommHistory::GroupModel(this);
    m_GroupModel->setResolveContacts(CommHistory::GroupManager::DoNotResolve);

    if (!m_GroupModel->getGroups()) {
        qCritical() << "Failed to request group ";
        delete m_GroupModel;
        m_GroupModel = 0;
    }
}

NotificationManager* NotificationManager::instance()
{
    if (!m_pInstance) {
        m_pInstance = new NotificationManager(QCoreApplication::instance());
    }

    return m_pInstance;
}

void NotificationManager::showNotification(const CommHistory::Event& event,
                      const QString &channelTargetId,
                      CommHistory::Group::ChatType chatType,
                      const QString &details)
{
    qDebug() << event.toString() << channelTargetId << chatType << details;
    Notification n;
    n.event = event;
    n.channelTargetId = channelTargetId;
    n.chatType = chatType;

    postedNotifications.append(n);
}


void NotificationManager::showVoicemailNotification(int count)
{
    voicemailNotifications = count;
}

void NotificationManager::playClass0SMSAlert()
{
}

void NotificationManager::requestClass0Notification(const CommHistory::Event &)
{
}

void NotificationManager::removeNotificationToken(const QString &token)
{
}

CommHistory::GroupModel* NotificationManager::groupModel()
{
    return m_GroupModel;
}

QContactManager* NotificationManager::contactManager()
{
    return m_pContactManager;
}
