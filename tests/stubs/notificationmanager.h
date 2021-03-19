#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QObject>
#include <CommHistory/Event>
#include <CommHistory/Group>

#include <QContact>
#include <QContactManager>

QTCONTACTS_USE_NAMESPACE

namespace CommHistory {
    class GroupModel;
}

namespace RTComLogger {

class NotificationManager : public QObject
{
    Q_OBJECT
public:
    explicit NotificationManager(QObject *parent = 0);

    static NotificationManager* instance();
    void showNotification(const CommHistory::Event& event,
                          const QString &channelTargetId = QString(),
                          CommHistory::Group::ChatType chatType = CommHistory::Group::ChatTypeP2P,
                          const QString &details = QString());

    void removeNotificationToken(const QString &token);
    void removeConversationNotifications(const CommHistory::Recipient &recipient,
                                         CommHistory::Group::ChatType chatType=CommHistory::Group::ChatType::ChatTypeP2P);
    CommHistory::GroupModel* groupModel();
    void showVoicemailNotification(int count);
    void playClass0SMSAlert();
    void requestClass0Notification(const CommHistory::Event &event);
    QContactManager* contactManager();


    int voicemailNotifications;
    struct Notification {
        CommHistory::Event event;
        QString channelTargetId;
        CommHistory::Group::ChatType chatType;
    };
    QList<Notification> postedNotifications;

    static NotificationManager* m_pInstance;
    QContactManager *m_pContactManager;
    CommHistory::GroupModel *m_GroupModel;
};

}
#endif // NOTIFICATIONMANAGER_H
