#ifndef MESSAGING_H
#define MESSAGING_H

#include "module.h"

class CoreMessagingModule : public CoreModule
{
    Q_OBJECT
public:
    CoreMessagingModule(QObject* parent, Tox* tox, QMutex* mutex);
    void update();

signals:
    void friendMessageReceived(int friendnumber, QString msg);

public slots:
    void sendMessage(int friendnumber, QString msg);

private:
    // callbacks -- userdata is always a pointer to an instance of this class
    static void callbackFriendMessage(Tox* tox, int32_t friendnumber, const uint8_t* message, uint16_t length, void* userdata);

private:

};

#endif // MESSAGING_H
